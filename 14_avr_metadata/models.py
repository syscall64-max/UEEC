"""Simulation clock and application pin; hardware identity comes from avr-gcc."""
from dataclasses import dataclass

@dataclass(frozen=True)
class Model:
    name: str
    clock_hz: int
    port: str
    pin: int

MODELS=(Model("attiny85",8_000_000,"B",0),
        Model("atmega328p",16_000_000,"B",5),
        Model("atmega2560",16_000_000,"B",7))
HALF_PERIOD_MS=100
ISA_MACROS=("__AVR_HAVE_MOVW__","__AVR_HAVE_LPMX__","__AVR_HAVE_ADIW__",
            "__AVR_HAVE_MUL__","__AVR_HAVE_JMP_CALL__","__AVR_HAVE_ELPM__",
            "__AVR_HAVE_ELPMX__","__AVR_HAVE_EIJMP_EICALL__")
