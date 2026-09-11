#!/usr/bin/env python3
"""Report the automated-AI-review state of every PR in the SFD manifest.

The ArduPilot dev-call reviewer posts its findings as a PR comment tagged
"Automated review note - AI-generated"; a superseded round is re-edited to
open with "Deprecated - see below for the updated review". A review is only
worth anything while it still matches the PR head, so for each PR this
prints the latest live round, the head it was taken at, and whether the PR
has moved underneath it.

Usage:
  ai_review_status.py                 # table over Tools/SFD/prs.txt
  ai_review_status.py 32768 33585     # just these PRs
  ai_review_status.py --detail 32768  # print the latest review body
  ai_review_status.py --sweep         # also list open PRs missing from the manifest
"""

import argparse
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

REPO = "ArduPilot/ardupilot"
AUTHOR = "andyp1per"
HERE = os.path.dirname(os.path.abspath(__file__))
MANIFEST = os.path.join(HERE, "prs.txt")

# A comment is an automated review round if it carries any of these.
REVIEW_MARKERS = (
    "automated review note",
    "uav.tridgell.net/devcallreviews",
    "devcall_pr_reviews",
)
DEPRECATED_MARKER = "deprecated"
# "Re-reviewed at head `x`" is the head the round was actually taken at; the bot
# adds "it is now at `y`" when the PR moved under it and it checked the delta.
HEAD_RE = re.compile(r"(?:re-)?reviewed at head\s+`([0-9a-f]{7,40})`", re.I)
ALSO_HEAD_RE = re.compile(r"(?:now at|verified at)\s+`([0-9a-f]{7,40})`", re.I)
# the verdict is stated in bold, in one of a handful of shapes; tried in order
# because the later ones are looser and would mis-read the earlier ones
V = r"(APPROVE|REQUEST CHANGES|COMMENT)"
VERDICT_RES = (
    re.compile(r"verdict moves from\s+\**(?:APPROVE|REQUEST CHANGES|COMMENT)\**\s+to\s+\**" + V, re.I),
    re.compile(r"(?:->|\u2192)\s*\**\s*" + V, re.I),
    re.compile(r"verdict\s+(?:stays|remains|is)\s+\**" + V, re.I),
    re.compile(r"verdict[:\s]+\**\s*" + V, re.I),
    re.compile(r"^#+\s*\**\s*" + V, re.I | re.M),
    re.compile(r"\*\*" + V + r"\s+stands", re.I),
    re.compile(r"[\u2014-]\s*\**" + V + r"\b", re.I),
    re.compile(r"\*\*" + V + r"[.,:]?\*\*", re.I),
    re.compile(r"no (?:merge )?blockers", re.I),
)


def gh_json(number, fields):
    out = subprocess.run(
        ["gh", "pr", "view", str(number), "--repo", REPO, "--json", ",".join(fields)],
        capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError(out.stderr.strip() or "gh failed for #%s" % number)
    return json.loads(out.stdout)


TAG = "AIReview"


def add_tag(number):
    """Ask for an automated review by labelling the PR.

    gh pr edit goes through GraphQL and fails on this repo with the Projects
    (classic) deprecation error, so the label goes on over REST.
    """
    out = subprocess.run(
        ["gh", "api", "-X", "POST", "repos/%s/issues/%d/labels" % (REPO, number),
         "-f", "labels[]=%s" % TAG],
        capture_output=True, text=True)
    return out.returncode == 0, (out.stderr.strip().splitlines() or [""])[0]


def read_manifest(path):
    prs = []
    for line in open(path):
        line = line.split("#")[0].strip()
        if line:
            prs.append(int(line))
    return prs


def is_review(body):
    low = body.lower()
    return any(m in low for m in REVIEW_MARKERS)


def deprecated(body):
    # the bot collapses a superseded round behind a leading quoted banner
    return DEPRECATED_MARKER in body[:400].lower()


def verdict_of(body):
    # the bot states the verdict in the first paragraphs; a later "blocking" or
    # "approve" in prose is not a verdict, so only the explicit forms count
    head = body[:3000]
    for rx in VERDICT_RES:
        m = rx.search(head)
        if m:
            if not m.groups():
                return "COMMENT", "no merge blockers"
            return m.group(1).upper(), m.group(0).strip("* ")
    return "?", ""


def collect(number):
    pr = gh_json(number, ["number", "title", "state", "headRefOid", "updatedAt",
                          "comments", "commits", "reviewDecision", "url", "labels"])
    head = pr["headRefOid"]
    commits = pr.get("commits") or []
    pushed = commits[-1]["committedDate"] if commits else None

    rounds = [c for c in pr.get("comments", []) if is_review(c["body"])]
    live = [c for c in rounds if not deprecated(c["body"])]
    latest = live[-1] if live else (rounds[-1] if rounds else None)

    info = {
        "number": number,
        "title": pr["title"],
        "state": pr["state"],
        "head": head,
        "pushed": pushed,
        "rounds": len(rounds),
        "decision": pr.get("reviewDecision") or "-",
        "url": pr["url"],
        "labels": [l["name"] for l in pr.get("labels", [])],
        "review_at": None,
        "review_head": None,
        "verdict": None,
        "headline": None,
        "stale": None,
        "body": None,
        "deprecated_only": bool(rounds) and not live,
    }
    if latest:
        body = latest["body"]
        info["body"] = body
        info["review_at"] = latest["createdAt"]
        info["author"] = latest["author"]["login"]
        m = HEAD_RE.search(body)
        info["review_head"] = m.group(1) if m else None
        info["also_heads"] = ALSO_HEAD_RE.findall(body)
        info["verdict"], info["headline"] = verdict_of(body)
        seen = [h for h in [info["review_head"]] + info["also_heads"] if h]
        if seen:
            info["stale"] = not any(head.startswith(h) for h in seen)
        elif pushed:
            info["stale"] = latest["createdAt"] < pushed
    return info


def fmt_row(i):
    if not i["rounds"]:
        state = "none"
    elif i["deprecated_only"]:
        state = "all superseded"
    elif i["stale"] is True:
        state = "STALE"
    elif i["stale"] is False:
        state = "current"
    else:
        state = "unknown"
    at = (i["review_at"] or "")[:10]
    rh = i["review_head"][:10] if i["review_head"] else ""
    return "%-7s %-14s %-4s %-10s %-11s %-16s %s" % (
        "#%d" % i["number"], state, i["rounds"] or "", at, rh,
        (i["verdict"] or "") if i["rounds"] else "", i["title"][:58])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("prs", nargs="*", type=int)
    ap.add_argument("--detail", type=int, help="print the latest review body for one PR")
    ap.add_argument("--tag", action="store_true",
                    help="add the %s label to every PR with no current review" % TAG)
    ap.add_argument("--sweep", action="store_true",
                    help="also list open PRs by %s that the manifest omits" % AUTHOR)
    args = ap.parse_args()

    if args.detail:
        i = collect(args.detail)
        if not i["body"]:
            print("#%d: no automated review found" % args.detail)
            return 0
        print("#%d %s\n%s\nreview %s by %s at head %s (PR head %s)\n" % (
            i["number"], i["title"], i["url"], i["review_at"], i.get("author"),
            i["review_head"], i["head"][:10]))
        print(i["body"])
        return 0

    numbers = args.prs or read_manifest(MANIFEST)
    with ThreadPoolExecutor(max_workers=8) as pool:
        results = list(pool.map(collect, numbers))

    if args.tag:
        want = [i for i in results
                if i["state"] == "OPEN" and TAG not in i["labels"]
                and (not i["rounds"] or i["stale"] or i["deprecated_only"])]
        print("labelling %d PRs with %s\n" % (len(want), TAG))
        for i in want:
            ok, err = add_tag(i["number"])
            print("  #%-6d %s%s" % (i["number"], "tagged" if ok else "FAILED",
                                    "" if ok else ": " + err))
        return 0

    print("%-7s %-14s %-4s %-10s %-11s %-16s %s" % (
        "PR", "AI REVIEW", "RNDS", "LAST", "AT HEAD", "VERDICT", "TITLE"))
    for i in results:
        print(fmt_row(i))

    missing = [i for i in results if not i["rounds"]]
    stale = [i for i in results if i["stale"] is True or i["deprecated_only"]]
    print("\n%d PRs: %d never AI-reviewed, %d reviewed but stale, %d current" % (
        len(results), len(missing), len(stale),
        len(results) - len(missing) - len(stale)))
    if missing:
        print("never reviewed: " + " ".join("#%d" % i["number"] for i in missing))
    if stale:
        print("stale:          " + " ".join("#%d" % i["number"] for i in stale))

    closed = [i for i in results if i["state"] != "OPEN"]
    if closed:
        print("not open:       " + " ".join(
            "#%d(%s)" % (i["number"], i["state"]) for i in closed))

    if args.sweep:
        out = subprocess.run(
            ["gh", "pr", "list", "--author", AUTHOR, "--repo", REPO, "--state", "open",
             "--json", "number,title", "--limit", "200"],
            capture_output=True, text=True, check=True)
        known = set(numbers)
        extra = [p for p in json.loads(out.stdout) if p["number"] not in known]
        print("\nopen PRs not in the manifest: %d" % len(extra))
        for p in extra:
            print("  #%-7d %s" % (p["number"], p["title"][:70]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
