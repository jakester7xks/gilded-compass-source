"""Generate the additive native compass strip from the original extracted HUD."""
from pathlib import Path
import json
import math
root = Path(__file__).resolve().parents[1]
out = root / 'out/native-heading'
source = root / 'local-game-ui'
out.mkdir(parents=True, exist_ok=True)
html = (source / 'minimaphudview2.html').read_bytes()
if html.count(b'</body>') != 1 or b'GildedCompassBar' in html:
    raise ValueError('Unexpected original HUD')
labels = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW']
markup = ['<div id="GildedCompassBar" css="inherit-no-pickable">',
          '<div id="GildedCompassRail"></div>',
          '<div id="GildedCompassRailEtching"></div>',
          '<div id="GildedCompassViewport">',
          '<div id="GildedCompassDirectionStrip" css="gc-direction-strip">']
styles = ['/* Gilded Compass: separate direction bar, original minimap unchanged. */',
 '#GildedCompassBar { position: absolute; left: 50%; top: 40px; translate-x: -50%; width: 600px; height: 60px; }',
 '#GildedCompassRail { position: absolute; left: 0px; top: 35px; width: 100%; height: 1px; background-color: #b48757; opacity: 0.8; }',
 '#GildedCompassViewport { position: absolute; left: 0px; top: 0px; width: 600px; height: 36px; overflow-x: hidden; overflow-y: hidden; }',
 '.gc-direction-strip { position: absolute; left: 0px; top: 0px; width: 3000px; height: 36px; translate-x: 0px; }',
 '.gc-direction-strip.gc-strip-unavailable { opacity: 0; }',
 '.gc-compass-letter { position: absolute; top: 0px; width: 60px; height: 25px; translate-x: -50%; text-align: center; text-stroke-width: 1px; text-stroke-color: #181818; }',
 '.gc-compass-tick { position: absolute; top: 29px; width: 1px; height: 5px; background-color: #b48757; }',
 '#GildedCompassCenter { position: absolute; left: 50%; top: 27px; translate-x: -50%; width: 2px; height: 14px; background-color: #dfd4c7; }',
 '#GildedCompassStatus { position: absolute; left: 0px; top: 43px; width: 100%; height: 20px; text-align: center; }']
for angle in range(-90, 451, 15):
    x = 300 + angle * 5
    name = f'GCCompassTick{angle+90}'
    markup.append(f'<div id="{name}" css="gc-compass-tick"></div>')
    styles.append(f'#{name} {{ left: {x}px; }}')
    if angle % 45 == 0:
        name = f'GCCompassLetter{angle+90}'
        markup.append(f'<div id="{name}" css="gc-compass-letter font-text-caption-minor-white">{labels[(angle//45)%8]}</div>')
        styles.append(f'#{name} {{ left: {x}px; }}')
markup += ['</div></div>', '<div id="GildedCompassCenter"></div>',
           '<div id="GildedCompassObjectiveViewport"><div id="GildedCompassObjective" css="gc-objective-marker"><div id="GildedCompassObjectiveDiamond"><div id="GildedCompassObjectiveGem"></div></div><div id="GildedCompassObjectiveDistance" css="gc-objective-distance font-text-caption-minor-white"></div></div></div>',
           '<div id="GildedCompassStatus" css="gc-heading-label font-text-caption-minor-white">Awaiting heading</div>', '</div>']
styles += [
 '#GildedCompassObjectiveViewport { position: absolute; left: 0px; top: 20px; width: 600px; height: 48px; overflow-x: hidden; overflow-y: hidden; }',
 '.gc-objective-marker { position: absolute; top: 0px; width: 80px; height: 48px; translate-x: -50%; opacity: 0; }',
 '.gc-objective-marker.gc-objective-hidden { opacity: 0; }',
 '#GildedCompassObjectiveDiamond { position: absolute; left: 50%; top: 2px; width: 8px; height: 8px; translate-x: -50%; rotate-z: 45deg; background-color: #b48757; }',
 '.gc-objective-distance { position: absolute; left: 0px; top: 23px; width: 100%; height: 20px; text-align: center; text-stroke-width: 1px; text-stroke-color: #181818; }']
styles += [
 '/* Approved Gilded Cartographer: open double rail, woven terminals at 150%. */',
 '#GildedCompassRail { top: 34px; height: 1px; background-color: #edcb87; opacity: 1; }',
 '#GildedCompassRailEtching { position: absolute; left: 0px; top: 37px; width: 600px; height: 1px; background-color: #9d753d; }',
 '.gc-compass-tick { background-color: #d6b475; }',
 '.gc-compass-letter { color: #f1dfb8; }',
 '#GildedCompassCenter { top: 30px; width: 1px; height: 5px; background-color: #edcb87; }',
 '#GildedCompassObjectiveDiamond { top: 8px; width: 16px; height: 16px; box-sizing: border-box; background-color: #29261fe0; border: #edcb87 1px solid; boundary-anti-aliasing: both; }',
 '#GildedCompassObjectiveGem { position: absolute; left: 4px; top: 4px; width: 6px; height: 6px; background-color: #f3d99f; boundary-anti-aliasing: both; }',
 '.gc-objective-distance { top: 30px; color: #f1dfb8; }']

# Native UI supports rotated rectangles, not SVG paths. Flatten cubic ribbons
# into short antialiased strokes: no extra texture registry or shared UI changes.
# Each terminal is 66 x 22.5 UI pixels (44 x 15 at the requested 150%).
def curve(points, steps=18):
    result = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        result.append(tuple(u**3*points[0][k] + 3*u*u*t*points[1][k]
                            + 3*u*t*t*points[2][k] + t**3*points[3][k] for k in (0, 1)))
    return result

paths = [
 [(0,0),(9,0),(12,-7),(18,-7)],
 [(18,-7),(26,-7),(29,10),(35,10)],
 [(35,10),(44,10),(44,-10),(35,-10)],
 [(35,-10),(29,-10),(26,7),(18,7)],
 [(18,7),(12,7),(9,0),(0,0)],
 [(8,0),(19,0),(22,-7),(27,-7)],
 [(27,-7),(34,-7),(36,0),(44,0)],
 [(44,0),(36,0),(34,7),(27,7)],
 [(27,7),(22,7),(19,0),(8,0)],
]
art = []
for side, origin, direction in [('L', 0, -1), ('R', 600, 1)]:
    # Spear terminal: gold facets taper from the loop to a fine point.
    for column in range(24):
        x = origin + direction * (42 + column)
        h = 5.5 * (1 - column / 24)
        name = f'GCFinial{side}{column}'
        art.append(f'<div id="{name}"></div>')
        styles.append(f'#{name} {{ position: absolute; left: {x:.3f}px; top: {35.5-h:.3f}px; width: 1.2px; height: {2*h:.3f}px; background-color: #dfb96e; boundary-anti-aliasing: both; }}')
    for layer, thickness, color in [('Shadow', 2.8, '#302519b0'), ('Gold', 1.25, '#edcb87')]:
        for p, controls in enumerate(paths):
            samples = curve(controls)
            for n, (a, b) in enumerate(zip(samples, samples[1:])):
                ax, ay = origin + direction*a[0], 35.5+a[1]*1.125
                bx, by = origin + direction*b[0], 35.5+b[1]*1.125
                length = math.hypot(bx-ax, by-ay) + 0.3
                angle = math.degrees(math.atan2(by-ay, bx-ax))
                name = f'GCWeave{side}{layer}{p}_{n}'
                art.append(f'<div id="{name}"></div>')
                styles.append(f'#{name} {{ position: absolute; left: {(ax+bx-length)/2:.3f}px; top: {(ay+by-thickness)/2:.3f}px; width: {length:.3f}px; height: {thickness}px; rotate-z: {angle:.3f}deg; background-color: {color}; boundary-anti-aliasing: both; }}')
# Keep filigree behind the moving marker, inside our custom root only.
# The bridge bounds child lists at 256 entries. Group static strokes so its
# guarded traversal can still reach the dynamic compass elements.
grouped = []
for start in range(0, len(art), 128):
    grouped += ['<div css="inherit-no-pickable">', *art[start:start+128], '</div>']
markup[1:1] = grouped
for x in range(601):
    styles.append(f'.gc-objective-marker.gc-objective-x-{x:03d} {{ left: {x}px; opacity: 1; }}')
for step in range(720):
    styles.append(f'.gc-direction-strip.gc-yaw-{step:03d} {{ translate-x: {-step*2.5:g}px; }}')
addition = ('\r\n' + '\r\n'.join(markup) + '\r\n').encode()
css_addition = ('\r\n'+'\r\n'.join(styles)+'\r\n').encode()
patched = html.replace(b'</body>', addition+b'</body>')
assert patched.replace(addition,b'',1) == html
(out/'minimaphudview2.html').write_bytes(patched)
css = (source/'minimaphudview2.css').read_bytes()
target = out/'resource-mod/Gilded Compass Heading Test/files/ui/xml/gamemain/play/minimaphudview2.css'
target.parent.mkdir(parents=True,exist_ok=True)
target.write_bytes(css+css_addition)
guards = out/'build-guards.json'
if guards.exists():
    data = json.loads(guards.read_text())
    data.update(state='DECORATED_SELECTED_DESTINATIONS_AWAITING_GAME_TEST', heading_source_user_verified=True,
                direction_strip_in_game_verified=True, quest_integration='experimental association only')
    guards.write_text(json.dumps(data,indent=2))
print('Generated direction strip and hidden objective-test elements; original HUD bytes preserved.')
