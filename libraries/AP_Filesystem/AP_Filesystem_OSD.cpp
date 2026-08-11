/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
  ArduPilot filesystem interface for the OSD shorthand table (@OSD/shorthand.dat)
 */

#include "AP_Filesystem_config.h"

#if AP_FILESYSTEM_OSD_ENABLED

#include "AP_Filesystem.h"
#include "AP_Filesystem_OSD.h"
#include <AP_OSD/AP_OSD.h>
#include <AP_Math/AP_Math.h>

extern const AP_HAL::HAL& hal;

#if CONFIG_HAL_BOARD != HAL_BOARD_QURT
extern int errno;
#endif

#define IDLE_TIMEOUT_MS 30000

bool AP_Filesystem_OSD::check_file_name(const char *name)
{
    return strcmp(name, "shorthand.dat") == 0;
}

int AP_Filesystem_OSD::open(const char *fname, int flags, bool allow_absolute_paths)
{
    if (!check_file_name(fname)) {
        errno = ENOENT;
        return -1;
    }
    AP_OSD *osd = AP::osd();
    if (osd == nullptr) {
        errno = ENOENT;
        return -1;
    }
    const bool readonly = ((flags & O_ACCMODE) == O_RDONLY);
    const uint32_t now = AP_HAL::millis();

    uint8_t idx;
    for (idx=0; idx<max_open_file; idx++) {
        if (file[idx].open && now - file[idx].last_op_ms > IDLE_TIMEOUT_MS) {
            file[idx].open = false;
            delete file[idx].writebuf;
            file[idx].writebuf = nullptr;
        }
        if (!readonly && file[idx].writebuf != nullptr) {
            // only one upload at a time
            errno = EBUSY;
            return -1;
        }
        if (!file[idx].open) {
            break;
        }
    }
    if (idx == max_open_file) {
        errno = ENFILE;
        return -1;
    }

    struct rfile &r = file[idx];
    r.open = true;
    r.file_ofs = 0;
    r.last_op_ms = now;
    if (readonly) {
        r.writebuf = nullptr;
        // snapshot the current table so reads are consistent for this handle
        r.blob_len = osd->shorthand().to_blob(r.blob);
    } else {
        r.writebuf = NEW_NOTHROW ExpandingString();
        r.blob_len = 0;
        if (r.writebuf == nullptr) {
            r.open = false;
            errno = ENOMEM;
            return -1;
        }
    }
    return idx;
}

int AP_Filesystem_OSD::close(int fd)
{
    if (fd < 0 || fd >= max_open_file || !file[fd].open) {
        errno = EBADF;
        return -1;
    }
    struct rfile &r = file[fd];
    r.open = false;
    bool ok = true;
    if (r.writebuf != nullptr) {
        AP_OSD *osd = AP::osd();
        // commit the uploaded blob into the table (and persist it)
        ok = osd != nullptr &&
             !r.writebuf->has_failed_allocation() &&
             osd->shorthand().from_blob((const uint8_t *)r.writebuf->get_string(),
                                        r.writebuf->get_length());
        delete r.writebuf;
        r.writebuf = nullptr;
    }
    if (!ok) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int32_t AP_Filesystem_OSD::read(int fd, void *buf, uint32_t count)
{
    if (fd < 0 || fd >= max_open_file || !file[fd].open) {
        errno = EBADF;
        return -1;
    }
    struct rfile &r = file[fd];
    if (r.writebuf != nullptr) {
        errno = EBADF;
        return -1;
    }
    r.last_op_ms = AP_HAL::millis();

    if (r.file_ofs >= r.blob_len) {
        return 0;
    }
    const uint32_t n = MIN(count, uint32_t(r.blob_len - r.file_ofs));
    memcpy(buf, &r.blob[r.file_ofs], n);
    r.file_ofs += n;
    return n;
}

int32_t AP_Filesystem_OSD::lseek(int fd, int32_t offset, int seek_from)
{
    if (fd < 0 || fd >= max_open_file || !file[fd].open) {
        errno = EBADF;
        return -1;
    }
    struct rfile &r = file[fd];
    switch (seek_from) {
    case SEEK_SET:
        r.file_ofs = offset;
        break;
    case SEEK_CUR:
        r.file_ofs += offset;
        break;
    case SEEK_END:
        r.file_ofs = r.blob_len + offset;
        break;
    }
    return r.file_ofs;
}

int32_t AP_Filesystem_OSD::write(int fd, const void *buf, uint32_t count)
{
    if (fd < 0 || fd >= max_open_file || !file[fd].open) {
        errno = EBADF;
        return -1;
    }
    struct rfile &r = file[fd];
    if (r.writebuf == nullptr) {
        errno = EBADF;
        return -1;
    }
    r.last_op_ms = AP_HAL::millis();

    // FTP may write out of order at arbitrary offsets: grow the buffer to cover
    // this write then copy into place (mirrors AP_Filesystem_Mission)
    if (r.file_ofs + count > r.writebuf->get_length()) {
        if (!r.writebuf->append(nullptr, r.file_ofs + count - r.writebuf->get_length())) {
            errno = ENOSPC;
            return -1;
        }
    }
    uint8_t *b = (uint8_t *)r.writebuf->get_writeable_string();
    memcpy(&b[r.file_ofs], buf, count);
    r.file_ofs += count;
    return count;
}

int AP_Filesystem_OSD::stat(const char *name, struct stat *stbuf)
{
    if (!check_file_name(name)) {
        errno = ENOENT;
        return -1;
    }
    AP_OSD *osd = AP::osd();
    if (osd == nullptr) {
        errno = ENOENT;
        return -1;
    }
    memset(stbuf, 0, sizeof(*stbuf));
    uint8_t tmp[AP_OSD_Shorthand::BLOB_MAX];
    stbuf->st_size = osd->shorthand().to_blob(tmp);
    return 0;
}

#endif  // AP_FILESYSTEM_OSD_ENABLED
