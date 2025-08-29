import time
from typing import Callable, Optional


def wait_for_condition(
        condition: Callable[[], bool],
        timeout_seconds: float,
        bail_condition: Callable[[], bool]) -> bool:
    start_time = time.monotonic()
    while time.monotonic() - start_time < timeout_seconds:
        if condition():
            return True
        if bail_condition():
            return False
        remaining_time = timeout_seconds - (time.monotonic() - start_time)
        sleep_time = min(timeout_seconds/10, remaining_time)
        if sleep_time > 0:
            time.sleep(sleep_time)
    return False
