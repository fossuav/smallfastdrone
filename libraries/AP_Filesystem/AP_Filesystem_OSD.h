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
  @OSD virtual filesystem: exposes the user OSD message shorthand table as a
  single binary blob ("shorthand.dat") so a ground station can read and write it
  over MAVLink FTP. Backed by AP_OSD's shorthand table (RAM + StorageManager),
  it needs no real filesystem or SD card. Modelled on AP_Filesystem_VTX.
 */
#pragma once

#include "AP_Filesystem_config.h"

#if AP_FILESYSTEM_OSD_ENABLED

#include "AP_Filesystem_backend.h"
#include <AP_Common/ExpandingString.h>
#include <AP_OSD/AP_OSD_Shorthand.h>

class AP_Filesystem_OSD : public AP_Filesystem_Backend
{
public:
    int open(const char *fname, int flags, bool allow_absolute_paths = false) override;
    int close(int fd) override;
    int32_t read(int fd, void *buf, uint32_t count) override;
    int32_t lseek(int fd, int32_t offset, int whence) override;
    int32_t write(int fd, const void *buf, uint32_t count) override;
    int stat(const char *pathname, struct stat *stbuf) override;

private:
    static constexpr uint8_t max_open_file = 2;

    struct rfile {
        bool open;
        uint32_t file_ofs;
        ExpandingString *writebuf;   // non-null while an upload is in progress
        uint32_t last_op_ms;
        // serialized snapshot of the table taken at open() for reads
        uint8_t blob[AP_OSD_Shorthand::BLOB_MAX];
        uint16_t blob_len;
    } file[max_open_file];

    bool check_file_name(const char *fname);
};

#endif  // AP_FILESYSTEM_OSD_ENABLED
