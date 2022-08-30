#include "gdswriter.h"
#include "gdsii.h"
using namespace gdstk;
GdsWriter gdswriter_init(const char* filename, const char* library_name, double unit,
                         double precision, uint64_t max_points, tm* timestamp,
                         ErrorCode* error_code) {
    GdsWriter result = {NULL, unit, precision, max_points};

    if (timestamp) {
        result.timestamp = *timestamp;
    } else {
        get_now(result.timestamp);
    }

    result.out = fopen(filename, "wb");
    if (result.out == NULL) {
        fputs("[GDSTK] Unable to open GDSII file for output.\n", stderr);
        if (error_code) *error_code = ErrorCode::OutputFileOpenError;
        return result;
    }

    uint64_t len = strlen(library_name);
    if (len % 2) len++;
    uint16_t buffer_start[] = {6,
                               0x0002,
                               0x0258,
                               28,
                               0x0102,
                               (uint16_t)(result.timestamp.tm_year + 1900),
                               (uint16_t)(result.timestamp.tm_mon + 1),
                               (uint16_t)result.timestamp.tm_mday,
                               (uint16_t)result.timestamp.tm_hour,
                               (uint16_t)result.timestamp.tm_min,
                               (uint16_t)result.timestamp.tm_sec,
                               (uint16_t)(result.timestamp.tm_year + 1900),
                               (uint16_t)(result.timestamp.tm_mon + 1),
                               (uint16_t)result.timestamp.tm_mday,
                               (uint16_t)result.timestamp.tm_hour,
                               (uint16_t)result.timestamp.tm_min,
                               (uint16_t)result.timestamp.tm_sec,
                               (uint16_t)(4 + len),
                               0x0206};
    big_endian_swap16(buffer_start, COUNT(buffer_start));
    fwrite(buffer_start, sizeof(uint16_t), COUNT(buffer_start), result.out);
    fwrite(library_name, 1, len, result.out);

    uint16_t buffer_units[] = {20, 0x0305};
    big_endian_swap16(buffer_units, COUNT(buffer_units));
    fwrite(buffer_units, sizeof(uint16_t), COUNT(buffer_units), result.out);
    uint64_t units[] = {gdsii_real_from_double(precision / unit),
                        gdsii_real_from_double(precision)};
    big_endian_swap64(units, COUNT(units));
    fwrite(units, sizeof(uint64_t), COUNT(units), result.out);
    return result;
}