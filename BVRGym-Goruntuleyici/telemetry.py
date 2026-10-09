"""Display estimates from recorded positions; never change source telemetry."""
from bisect import bisect_right
import math


def estimate_motion(rows, to_cartesian):
    if not rows:
        return
    times = [row['sim_time_s'] for row in rows]
    points = [to_cartesian(row['latitude_deg'], row['longitude_deg'], row['altitude_m']) for row in rows]

    def position(time):
        index = min(len(rows) - 1, max(0, bisect_right(times, time) - 1))
        if index == len(rows) - 1:
            return points[index]
        fraction = (time - times[index]) / (times[index + 1] - times[index])
        return tuple(a + (b - a) * fraction for a, b in zip(points[index], points[index + 1]))

    for row in rows:
        current = row['sim_time_s']
        begin, end = max(times[0], current - 1.0), current
        if end == begin:
            end = min(times[-1], begin + 1.0)
        speed = None
        if end > begin:
            speed = math.dist(position(begin), position(end)) / (end - begin)
        row['estimated_tas_mps'] = speed if row['true_airspeed_mps'] is None else None
        temperature = standard_temperature(row['altitude_m'])
        reference_speed = row['true_airspeed_mps'] if row['true_airspeed_mps'] is not None else speed
        row['estimated_mach'] = (reference_speed / math.sqrt(1.4 * 287.05287 * temperature)
                                 if row['mach'] is None and reference_speed is not None and temperature else None)


def standard_temperature(altitude):
    # ISA layers, temperature in kelvin; estimates outside the model stay unknown.
    if not -2000 <= altitude <= 84852:
        return None
    levels = (0, 11000, 20000, 32000, 47000, 51000, 71000)
    temperatures = (288.15, 216.65, 216.65, 228.65, 270.65, 270.65, 214.65)
    gradients = (-.0065, 0, .001, .0028, 0, -.0028, -.002)
    index = max(0, bisect_right(levels, altitude) - 1)
    return temperatures[index] + gradients[index] * (altitude - levels[index])
