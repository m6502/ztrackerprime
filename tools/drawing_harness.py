"""Build small harnesses from current production function bodies, without the app.

Functions are copied verbatim into separate translation units (no LTO), so
benchmarks exercise the checked-out implementation and its real type layouts.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def function(path, signature):
    source = (ROOT / path).read_text()
    start = source.index(signature)
    # Ignore braces inside comments and literals, preserving character offsets.
    masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda m: ' ' * len(m[0]), source)
    opening = masked.index('{', start)
    depth = 0
    for end in range(opening, len(masked)):
        if masked[end] == '{':
            depth += 1
        elif masked[end] == '}':
            depth -= 1
            if depth == 0:
                return source[start:end + 1] + '\n'
    raise ValueError(f'Unclosed function: {signature}')
