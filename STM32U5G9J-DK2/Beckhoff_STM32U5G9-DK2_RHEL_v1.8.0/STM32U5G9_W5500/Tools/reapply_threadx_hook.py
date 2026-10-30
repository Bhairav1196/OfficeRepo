#!/usr/bin/env python3
"""Reapply the single v2 application hook after CubeMX regeneration.

@author Prathamesh Chipkar
@version 2.0.0-alpha.9.0
"""
from pathlib import Path
import re
import sys


def patch(root: Path) -> None:
    p = root / 'Core' / 'Src' / 'app_threadx.c'
    if not p.exists():
        raise SystemExit(f'Not found: {p}')
    text = p.read_text(encoding='utf-8')

    # Remove legacy hook fragments if they survived an old project iteration.
    text = text.replace('#include "master_threadx.h"\n', '')
    text = re.sub(r'\s*ret\s*=\s*Master_ThreadX_Create\(\);\s*\n\s*if\s*\(ret\s*!=\s*TX_SUCCESS\)\s*\{\s*return\s+ret;\s*\}', '', text)

    if '#include "app_rtos.h"' not in text:
        marker = '/* USER CODE BEGIN Includes */'
        if marker not in text:
            raise SystemExit('Includes USER CODE marker not found')
        text = text.replace(marker, marker + '\n#include "app_rtos.h"', 1)

    if 'fn_AppRtos_Create()' not in text:
        marker = '/* USER CODE BEGIN App_ThreadX_Init */'
        hook = '\n  ret = fn_AppRtos_Create();\n  if (ret != TX_SUCCESS)\n  {\n    return ret;\n  }'
        if marker in text:
            text = text.replace(marker, marker + hook, 1)
        else:
            raise SystemExit('App_ThreadX_Init USER CODE marker not found')

    if text.count('fn_AppRtos_Create()') != 1:
        raise SystemExit('Application RTOS hook must occur exactly once')
    p.write_text(text, encoding='utf-8')
    print('THREADX HOOK: PASS')


if __name__ == '__main__':
    patch(Path(sys.argv[1] if len(sys.argv) > 1 else '.').resolve())
