#include "telemetry.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>

// Debugging exercise notes:
// this file intentionally contains four runtime defects.
// The defects are related to malformed input shape, invalid numeric values,
// unsafe time deltas, and empty logs. Exact locations are not marked on purpose.

const int EXPECTED_FIELD_COUNT = 7;
const int MAX_LINE_LENGTH = 256;

int split_line(char line[], char* fields[], int max_fields) {
    int count = 0;
    char* cursor = line;

    while (*cursor != '\0' && count < max_fields) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r') {
            *cursor = '\0';
            ++cursor;
        }

        if (*cursor == '\0') {
            break;
        }

        fields[count] = cursor;
        ++count;

        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\n' &&
               *cursor != '\r') {
            ++cursor;
        }
    }

    return count;
}

template<typename Type>
std::optional<Type> parse(const char* text);

template<>
std::optional<long> parse<long>(const char* text) {
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);

    if (end == text) {
        return { };
    }

    return { value };
}

template<>
std::optional<int> parse<int>(const char* text) {
    const std::optional<long> long_parse_result = parse<long>(text);
    return long_parse_result.has_value() ?
        std::optional<int>{ static_cast<int>(long_parse_result.value()) } :
        std::optional<int>{ };
}

template<>
std::optional<double> parse<double>(const char* text) {
    char* end = nullptr;
    const double value = std::strtod(text, &end);

    if (end == text) {
        return { };
    }

    return { value };
}

#define ParseFrameField(M_FrameVarName, M_FieldName, M_FieldStringValue)                        \
{                                                                                               \
    using FieldType = decltype(M_FrameVarName.M_FieldName);                                     \
    const std::optional<FieldType> parse_result = parse<FieldType>(M_FieldStringValue);         \
    if (!parse_result.has_value()) {                                                            \
        std::cerr << "Error: Frame data contains not number value for "                         \
                << #M_FieldName << std::endl;                                                   \
        return { };                                                                             \
    }                                                                                           \
                                                                                                \
    M_FrameVarName.M_FieldName = parse_result.value();                                          \
}

std::optional<Frame> parse_frame(char line[]) {
    char* fields[EXPECTED_FIELD_COUNT] = {};
    const int field_count = split_line(line, fields, EXPECTED_FIELD_COUNT);

    if (field_count != EXPECTED_FIELD_COUNT) {
        std::cerr << "Error: Frame expected " << EXPECTED_FIELD_COUNT
                  << " fields, but got " << field_count << std::endl;
        return { };
    }

    Frame frame{ };
    
    ParseFrameField(frame, timestamp_ms,  fields[0])
    ParseFrameField(frame, seq,           fields[1])
    ParseFrameField(frame, voltage_v,     fields[2])
    ParseFrameField(frame, current_a,     fields[3])
    ParseFrameField(frame, temperature_c, fields[4])
    ParseFrameField(frame, gps_fix,       fields[5])
    ParseFrameField(frame, satellites,    fields[6])
    
    return frame;
}

double compute_frame_rate_hz(const Frame frames[], int frame_count) {
    const long elapsed_ms = frames[frame_count - 1].timestamp_ms - frames[0].timestamp_ms;

    return static_cast<double>((frame_count - 1) * 1000 / elapsed_ms);
}

std::optional<int> read_frames(const char* path, Frame frames[], int max_frames) {
    std::ifstream input{path};
    if (!input) {
        std::cerr << "Error: Failed to open input file: " << path << std::endl;
        return std::optional<int>{ };
    }

    int frame_count = 0;
    char line[MAX_LINE_LENGTH];

    while (input.getline(line, MAX_LINE_LENGTH)) {
        if (line[0] == '\0') {
            continue;
        }

        if (frame_count >= max_frames) {
            std::cerr << "Error: Lines count is more then maximum supported " << max_frames << std::endl;
            return { };
        }
        
        const std::optional<Frame> frame = parse_frame(line);
        if (!frame) {
            std::cerr << "Error: Malformed frame found at index: " << frame_count << std::endl;
            return { };
        }

        frames[frame_count] = frame.value();
        ++frame_count;
    }

    if (frame_count == 0) {
        std::cerr << "Error: File contains no frames" << std::endl;
        return { };
    }

    return { frame_count };
}

bool validate_frames(const Frame frames[], int frame_count) {
    if (frame_count <= 0) {
        std::cerr << "Error: No frames passed for validation" << std::endl;
        return false;
    }

    long last_time_stamp = frames[0].timestamp_ms;
    for (int i = 1; i < frame_count; ++i) {
        const long current_time_stamp = frames[i].timestamp_ms;
        
        if (last_time_stamp >= current_time_stamp) {
            std::cerr << "Error: Time stamp of frame with index " << i << " is invalid:" <<
            " less then of previous frame" << std::endl;
            return false;
        }
    }

    return true;
}

Summary summarize(const Frame frames[], int frame_count) {
    Summary summary{};
    summary.frames_total = frame_count;
    summary.frames_valid = frame_count;
    summary.voltage_min = frames[0].voltage_v;
    summary.voltage_max = frames[0].voltage_v;
    summary.low_voltage_frames = 0;

    double temperature_sum = 0.0;

    for (int i = 0; i < frame_count; ++i) {
        if (frames[i].voltage_v < summary.voltage_min) {
            summary.voltage_min = frames[i].voltage_v;
        }

        if (frames[i].voltage_v > summary.voltage_max) {
            summary.voltage_max = frames[i].voltage_v;
        }

        temperature_sum += frames[i].temperature_c;

        if (frames[i].voltage_v < 22.0) {
            ++summary.low_voltage_frames;
        }
    }

    const int temperature_tenths = static_cast<int>(temperature_sum * 10.0) / frame_count;
    summary.temperature_avg = static_cast<double>(temperature_tenths) / 10.0;
    summary.frame_rate_hz = compute_frame_rate_hz(frames, frame_count);
    return summary;
}

void print_summary(const Summary& summary) {
    std::cout << "frames_total " << summary.frames_total << std::endl;
    std::cout << "frames_valid " << summary.frames_valid << std::endl;
    std::cout << "voltage_min " << summary.voltage_min << std::endl;
    std::cout << "voltage_max " << summary.voltage_max << std::endl;
    std::cout << "temperature_avg " << summary.temperature_avg << std::endl;
    std::cout << "low_voltage_frames " << summary.low_voltage_frames << std::endl;
    std::cout << "frame_rate_hz " << summary.frame_rate_hz << std::endl;
}
