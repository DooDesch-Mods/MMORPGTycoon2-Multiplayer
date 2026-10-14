"""Canonical linked and saved root-inline Building identities."""
import re

UUID_PATTERN = r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"
NONNEGATIVE = r"(?:0|[1-9][0-9]{0,9})"
ROOT_PATTERN = r"inline:region\{" + NONNEGATIVE + r"\}/buildingManager/building\[" + NONNEGATIVE + r"\]"
KEY_PATTERN = r"(?:[0-9a-f]{64}:world:building:(?:[1-9][0-9]{0,18}|" + ROOT_PATTERN + r")|action:" + UUID_PATTERN + r")"


def valid_building_key(value):
    if not isinstance(value, str) or re.fullmatch(KEY_PATTERN, value) is None:
        return False
    if value.startswith('action:'):
        return True
    suffix = value[80:]
    if suffix.startswith('inline:'):
        region, child = re.fullmatch(r"inline:region\{([0-9]+)\}/buildingManager/building\[([0-9]+)\]", suffix).groups()
        return int(region) <= 2147483647 and int(child) <= 2147483647
    return int(suffix) <= 9223372036854775807
