"""Connect to a running KiCad instance over the IPC API.

Must be run with KiCad's bundled interpreter, which is the one that ships
with kipy installed:

    "C:/Program Files/KiCad/10.0/bin/python.exe" tools/kicad_ipc.py

Requires "Enable KiCad API" (Preferences > Plugins) and KiCad running.
"""

import sys

from kipy import KiCad
from kipy.errors import ApiError


def connect() -> KiCad:
    """Return a live KiCad handle, or exit with a readable reason."""
    try:
        k = KiCad()
        k.ping()
        return k
    except ApiError as e:
        sys.exit(
            f"Could not reach KiCad over IPC: {e}\n"
            "Check that KiCad is running and that Preferences > Plugins > "
            "'Enable KiCad API' is ticked."
        )


def main() -> None:
    k = connect()
    print(f"KiCad {k.get_version()}  (API {k.get_api_version()})")

    try:
        board = k.get_board()
    except ApiError as e:
        sys.exit(f"No board available - open the PCB editor first ({e})")

    footprints = board.get_footprints()
    nets = board.get_nets()
    print(f"board       : {board.name}")
    print(f"footprints  : {len(footprints)}")
    print(f"nets        : {len(nets)}")

    for fp in footprints:
        pos = fp.position  # nanometres
        print(f"  {fp.reference_field.value:8} {fp.value_field.value:16}"
              f" @ ({pos.x / 1e6:.2f}, {pos.y / 1e6:.2f}) mm")


if __name__ == "__main__":
    main()
