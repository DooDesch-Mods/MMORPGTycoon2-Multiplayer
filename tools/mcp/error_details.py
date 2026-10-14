"""Preserve nested asynchronous failures and their original causes."""
import traceback


def error_details(error: BaseException, depth: int = 0, seen=None) -> dict:
    seen = set() if seen is None else seen
    result = {"type": type(error).__name__, "message": str(error)}
    if depth >= 8 or id(error) in seen:
        result["bounded_chain"] = True
        return result
    seen.add(id(error))
    if isinstance(error, BaseExceptionGroup):
        result["exceptions"] = [error_details(child, depth + 1, seen) for child in error.exceptions]
    if error.__cause__ is not None:
        result["cause"] = error_details(error.__cause__, depth + 1, seen)
    elif error.__context__ is not None and not error.__suppress_context__:
        result["context"] = error_details(error.__context__, depth + 1, seen)
    return result


def original_causes(details: dict) -> list[str]:
    children = list(details.get("exceptions", []))
    children += [details[key] for key in ("cause", "context") if key in details]
    if children:
        return [message for child in children for message in original_causes(child)]
    return [f"{details['type']}: {details['message']}"]


def full_trace(error: BaseException) -> str:
    return "".join(traceback.format_exception(error))
