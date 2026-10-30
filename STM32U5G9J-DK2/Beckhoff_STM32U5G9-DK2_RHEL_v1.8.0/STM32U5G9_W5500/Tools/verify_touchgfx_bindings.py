#!/usr/bin/env python3
"""Verify the v2 single-unit maritime TouchGFX binding contract.

@author Prathamesh Chipkar
@version 2.0.0-alpha.9.0
"""
from pathlib import Path
import json

root = Path(__file__).resolve().parents[1]
p = next((root / 'TouchGFX').glob('*.touchgfx'))
data = json.loads(p.read_text(encoding='utf-8'))
screen = data['Application']['Screens'][0]
components_list = screen['Components']
components = {x['Name'] for x in components_list}
expected = ['tcpValue','uptimeValue','unit1Button','unit2Button','unit1Label','unit2Label','unitDescription','analogTitle']
for prefix in ('di','do'):
    for ch in range(1,17):
        expected += [f'{prefix}State{ch:02d}', f'{prefix}Name{ch:02d}', f'{prefix}Value{ch:02d}']
for ch in range(1,5):
    expected += [f'analogName{ch:02d}', f'analogValue{ch:02d}', f'aoMinus{ch:02d}', f'aoPlus{ch:02d}']
missing = sorted(set(expected) - components)
assert data['Application']['Resolution'] == {'Width':800,'Height':480}
assert len(data['Application']['Screens']) == 1
assert screen['Name'] == 'Screen1'
assert len(components_list) == 231
assert len(components) == len(components_list)
for prefix, count in (('navTile', 6), ('navLabel', 6), ('aiTrack', 4),
                      ('aiFill', 4), ('aoCard', 4), ('aoTrack', 4),
                      ('aoFill', 4), ('aoName', 4), ('aoValue', 4)):
    assert all(f'{prefix}{i:02d}' in components for i in range(1, count + 1))
assert sum(1 for x in components_list if x.get('Type') == 'Button') == 10
assert len(screen.get('Interactions', [])) == 10
if missing:
    print('FAIL missing widgets:', missing)
    raise SystemExit(1)
texts = (root/'TouchGFX/assets/texts/texts.xml').read_text(encoding='utf-8')
assert 'Font="Roboto-Regular.ttf"' in texts
assert 'Font="Roboto-Medium.ttf"' in texts
assert 'verdana' not in texts.lower()
functions = {x.get('Action', {}).get('FunctionName') for x in screen.get('Interactions', [])}
assert {'selectAnalogInputs', 'selectAnalogOutputs'}.issubset(functions)
view_h = (root/'TouchGFX/gui/include/gui/screen1_screen/Screen1View.hpp').read_text(encoding='utf-8')
view = (root/'TouchGFX/gui/src/screen1_screen/Screen1View.cpp').read_text(encoding='utf-8')
assert 'showPage(uint8_t page)' in view_h
assert all(x in view for x in ('MOTOR STARTER PANEL','ICGS VESSEL SENSORS HUB','FIN STABILIZER LOCAL CONTROL','MACHINERY CONTROL ROOM','BRIDGE CONSOLE'))
assert 'frame.aiValid[slot]' in view and 'frame.aoValid[slot]' in view
print(f'PASS: {len(components_list)} Designer components, {len(expected)} required bindings, six dark maritime pages, simultaneous AI/AO and Roboto typography.')
