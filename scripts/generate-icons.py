#!/usr/bin/env python3
"""Author CSSE's vector icons and antialiased PNG resource strips.

Optional development tool: requires Pillow, not used by the Windows build.
All geometry in this file is original project artwork; no downloaded art.
"""

import json
import math
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1] / 'CSSE' / 'res' / 'icons'
PALETTES = {
    'dark': dict(fg='#dce7f6', blue='#6eafff', green='#56d8ab', amber='#f6c86c', red='#ff899c', purple='#b8a0ff', soft='#56708e'),
    'light': dict(fg='#33465f', blue='#2767c8', green='#12836b', amber='#a87018', red='#cf405d', purple='#7553b8', soft='#98adc7'),
}


class Canvas:
    def __init__(self, palette, pixels=16, height=None):
        self.p = palette
        self.w, self.h = pixels, height or pixels
        self.image = Image.new('RGBA', (self.w * 8, self.h * 8))
        self.draw = ImageDraw.Draw(self.image)
        self.svg = []

    def color(self, value): return self.p.get(value, value)
    def xy(self, p): return (round(p[0] * self.w / 3), round(p[1] * self.h / 3))

    def line(self, points, color='fg', width=1.8):
        color = self.color(color)
        pts = [self.xy(p) for p in points]
        thick = max(1, round(width * self.w / 3))
        self.draw.line(pts, fill=color, width=thick, joint='curve')
        radius = thick / 2
        for x, y in pts:
            self.draw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=color)
        self.svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in points)}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>')

    def rect(self, box, color='fg', fill=None, radius=2, width=1.7):
        stroke = self.color(color) if color else None
        fill = self.color(fill) if fill else None
        a, b = self.xy(box[:2]), self.xy(box[2:])
        self.draw.rounded_rectangle((*a, *b), radius=round(radius*self.w/3), fill=fill, outline=stroke, width=max(1, round(width*self.w/3)))
        x,y,right,bottom = box
        self.svg.append(f'<rect x="{x}" y="{y}" width="{right-x}" height="{bottom-y}" rx="{radius}" fill="{fill or "none"}" stroke="{stroke or "none"}" stroke-width="{width}"/>')

    def ellipse(self, box, color='fg', fill=None, width=1.7):
        stroke = self.color(color) if color else None
        fill = self.color(fill) if fill else None
        self.draw.ellipse((*self.xy(box[:2]), *self.xy(box[2:])), outline=stroke, fill=fill, width=max(1, round(width*self.w/3)))
        x,y,right,bottom = box
        self.svg.append(f'<ellipse cx="{(x+right)/2}" cy="{(y+bottom)/2}" rx="{(right-x)/2}" ry="{(bottom-y)/2}" fill="{fill or "none"}" stroke="{stroke or "none"}" stroke-width="{width}"/>')

    def poly(self, points, fill='blue', color=None):
        fill = self.color(fill)
        self.draw.polygon([self.xy(p) for p in points], fill=fill)
        if color: self.line(points+[points[0]], color, 1.5)
        self.svg.append(f'<polygon points="{" ".join(f"{x},{y}" for x,y in points)}" fill="{fill}"/>')

    def finish(self):
        return self.image.resize((self.w, self.h), Image.Resampling.LANCZOS)

    def document(self):
        return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">' + ''.join(self.svg) + '</svg>\n'


def glyph(c, name):
    if name == 'folder':
        c.poly([(3,5),(10,5),(12,8),(21,8),(21,19),(3,19)], 'amber')
        c.line([(3,10),(21,10)], 'fg', 1.3)
    elif name in ('save', 'saveas'):
        c.rect((4,3,20,21), 'fg', radius=2)
        c.rect((8,3,16,9), None, 'blue' if name=='save' else 'green', radius=0)
        c.rect((8,14,16,21), 'fg', radius=1, width=1.3)
    elif name in ('gear', 'sun'):
        c.ellipse((8,8,16,16), 'amber' if name=='sun' else 'blue', 'amber' if name=='sun' else None)
        for angle in range(0,360,45):
            a=math.radians(angle)
            c.line([(12+7*math.cos(a),12+7*math.sin(a)),(12+9*math.cos(a),12+9*math.sin(a))], 'amber' if name=='sun' else 'fg')
    elif name in ('undo', 'redo'):
        pts=[(4,10),(8,6),(16,6),(20,10),(20,17)]
        if name=='redo':pts=[(24-x,y) for x,y in pts]
        c.line(pts, 'blue')
        c.line([(4,5),(4,10),(9,10)] if name=='undo' else [(20,5),(20,10),(15,10)], 'fg')
    elif name in ('grid', 'graph'):
        c.rect((3,3,21,21), 'soft', radius=2)
        c.line([(12,3),(12,21)], 'soft', 1.2);c.line([(3,12),(21,12)], 'soft', 1.2)
        if name=='graph':
            c.line([(6,17),(11,7),(18,14)], 'blue', 1.5)
            for x,y in [(6,17),(11,7),(18,14)]:c.ellipse((x-1.7,y-1.7,x+1.7,y+1.7), None, 'blue')
        else:c.ellipse((9,9,15,15), None, 'blue')
    elif name in ('brush', 'sword', 'pick', 'probe', 'bone'):
        c.line([(6,19),(18,5)], 'fg', 3 if name=='bone' else 2.5)
        if name=='sword':c.line([(5,14),(11,19)], 'amber', 2.4);c.line([(4,21),(7,18)], 'amber', 2.6)
        elif name in ('pick','probe'):c.line([(14,5),(20,4),(20,8)], 'blue', 1.7)
        elif name=='brush':c.poly([(4,17),(9,19),(6,22),(2,22)], 'blue')
        else:
            for x,y in [(5,20),(18,5)]:c.ellipse((x-2,y-2,x+2,y+2),None,'fg')
    elif name in ('terrain','house'):
        if name=='terrain':c.poly([(2,20),(8,8),(14,16),(18,11),(22,20)], 'green');c.ellipse((15,3,20,8),None,'amber')
        else:c.poly([(2,11),(12,3),(22,11)], 'amber');c.rect((5,11,19,21),'fg',radius=0);c.rect((10,15,14,21),None,'blue',radius=0)
    elif name=='fog':
        c.line([(5,10),(7,6),(12,5),(16,9),(20,10)], 'soft', 2.7)
        c.line([(3,15),(21,15)], 'fg', 1.5);c.line([(6,20),(18,20)], 'blue', 1.5)
    elif name=='dialog':
        c.rect((3,4,21,17),'blue',radius=3);c.line([(8,17),(5,21),(5,17)], 'blue')
        for x in [7,12,17]:c.ellipse((x-0.7,9.3,x+0.7,10.7),None,'fg')
    elif name=='code':
        c.line([(8,7),(3,12),(8,17)], 'blue');c.line([(16,7),(21,12),(16,17)], 'blue');c.line([(14,4),(10,20)], 'fg',1.6)
    elif name=='speaker':
        c.poly([(3,9),(7,9),(13,4),(13,20),(7,15),(3,15)], 'blue')
        c.line([(17,8),(19,12),(17,16)], 'fg');c.line([(20,5),(23,12),(20,19)], 'soft',1.4)
    elif name in ('game','openmw'):
        c.ellipse((3,3,21,21),'amber' if name=='game' else 'blue',width=1.6)
        c.poly([(12,5),(15,12),(12,19),(9,12)], 'amber' if name=='game' else 'blue')
        c.line([(5,12),(19,12)], 'fg',1.2)
    elif name in ('play','playred','next','fast'):
        fill='red' if name=='playred' else 'green' if name=='play' else 'blue'
        c.poly([(5,4),(19,12),(5,20)], fill)
        if name=='next':c.line([(20,5),(20,19)], 'fg')
        if name=='fast':c.poly([(13,4),(23,12),(13,20)],'blue')
    elif name=='previous':
        c.poly([(19,4),(5,12),(19,20)],'blue');c.line([(3,5),(3,19)],'fg')
    elif name=='stop':c.rect((5,5,19,19),None,'red',radius=2)
    elif name=='pause':c.rect((5,4,9,20),None,'fg',radius=1);c.rect((15,4,19,20),None,'fg',radius=1)
    elif name=='reset':
        c.line([(19,8),(15,4),(8,4),(4,9),(4,16),(9,20),(16,20),(20,16)],'blue')
        c.line([(19,3),(19,8),(14,8)],'fg')
    elif name=='delete':c.line([(5,5),(19,19)],'red',2.7);c.line([(19,5),(5,19)],'red',2.7)
    elif name=='plus':c.line([(4,12),(20,12)],'green',2.5);c.line([(12,4),(12,20)],'green',2.5)
    elif name in ('down','left','right'):
        pts=[(12,3),(12,20)];head=[(6,14),(12,20),(18,14)]
        if name!='down':
            convert=lambda p:(p[1],p[0]) if name=='right' else (24-p[1],p[0])
            pts=list(map(convert,pts));head=list(map(convert,head))
        c.line(pts,'blue',2.3);c.line(head,'blue',2.3)
    elif name in ('shield','armor'):
        c.poly([(12,3),(21,6),(19,16),(12,22),(5,16),(3,6)],'blue')
        c.line([(12,5),(12,19)],'fg',1.5);c.line([(6,10),(18,10)],'fg',1.4)
    elif name=='book':
        c.rect((5,3,20,21),'fg',radius=2);c.line([(8,3),(8,21)],'red',2);c.line([(11,8),(17,8)],'blue',1.4);c.line([(11,12),(17,12)],'soft',1.4)
    elif name=='shirt':c.poly([(8,3),(16,3),(22,8),(18,12),(17,10),(17,21),(7,21),(7,10),(6,12),(2,8)],'purple')
    elif name=='chest':
        c.rect((3,7,21,20),'amber',radius=2);c.line([(3,12),(21,12)],'fg',1.3);c.rect((10,10,14,15),None,'blue',radius=0)
    elif name=='door':
        c.rect((5,2,19,22),'amber',radius=1);c.rect((8,5,16,19),'fg',radius=0,width=1.2);c.ellipse((14,12,16,14),None,'blue')
    elif name=='leaf':
        c.poly([(11,22),(5,13),(6,5),(16,2),(20,10),(16,17)],'green');c.line([(10,19),(15,6)],'fg',1.3)
    elif name=='bulb':
        c.ellipse((6,2,18,15),'amber','amber');c.line([(9,18),(15,18)],'fg');c.line([(10,21),(14,21)],'soft',1.6)
    elif name=='hammer':
        c.line([(7,21),(17,7)],'fg',2.8);c.poly([(11,4),(15,2),(23,8),(19,13)],'amber')
    elif name in ('person','face'):
        if name=='face':c.rect((2,2,22,22),'soft',radius=4,width=1.4)
        c.ellipse((8,3,16,11),None,'blue');c.poly([(4,21),(6,15),(10,13),(14,13),(18,15),(20,21)],'blue')
    elif name=='paw':
        for x,y in [(4,8),(9,4),(15,4),(20,8)]:c.ellipse((x-2.2,y-2.2,x+2.2,y+2.2),None,'amber')
        c.ellipse((5,11,19,22),None,'amber')
    elif name in ('potion','apparatus'):
        c.line([(9,2),(15,2)],'fg',2)
        c.poly([(10,3),(14,3),(14,9),(21,20),(19,22),(5,22),(3,20),(10,9)],'blue' if name=='potion' else 'purple')
        c.line([(7,16),(17,16)],'fg',1.3);c.ellipse((10,17,12,19),None,'fg')
    elif name=='spark':
        c.poly([(12,1),(15,9),(23,12),(15,15),(12,23),(9,15),(1,12),(9,9)],'purple')
    elif name=='question':
        c.line([(7,7),(9,3),(15,3),(18,7),(15,11),(12,13),(12,15)],'amber',2.3);c.ellipse((10.8,19,13.2,21.4),None,'fg')
    elif name=='camera':
        c.rect((2,7,22,21),'fg',radius=3);c.poly([(6,7),(9,3),(15,3),(18,7)],'fg');c.ellipse((8,10,16,18),'blue',width=2)
    elif name=='search':c.ellipse((3,3,16,16),'blue',width=2);c.line([(15,15),(22,22)],'fg',2.5)
    elif name=='cube':
        c.poly([(12,2),(22,7),(22,17),(12,22),(2,17),(2,7)],'blue');c.line([(2,7),(12,12),(22,7)],'fg',1.2);c.line([(12,12),(12,22)],'fg',1.2)
    elif name=='world':
        c.ellipse((2,2,22,22),'blue',width=2);c.ellipse((8,2,16,22),'green',width=1.3);c.line([(3,9),(21,9)],'soft',1.2);c.line([(3,15),(21,15)],'soft',1.2)
    elif name=='hand':
        c.poly([(3,12),(6,13),(6,5),(9,5),(9,11),(10,3),(13,3),(13,11),(15,5),(18,5),(18,12),(21,11),(21,17),(17,22),(8,22),(3,17)],'blue')
    elif name=='plane':
        c.poly([(11,2),(13,2),(14,10),(23,15),(23,18),(14,15),(14,20),(18,22),(6,22),(10,20),(10,15),(1,18),(1,15),(10,10)],'blue')
    elif name in ('checked','unchecked'):
        c.rect((3,3,21,21),'green' if name=='checked' else 'soft',radius=4,width=1.7)
        if name=='checked':c.line([(7,12),(10,15),(17,8)],'green',2.3)
    else:raise ValueError(name)


STRIPS = {
    'toolbar_layers': ['save','plus','delete','question'],
    'toolbar_main': ['folder','save','gear','undo','redo','grid','brush','terrain','graph','sun','fog','dialog','code','speaker','game','openmw'],
    'toolbar_render': ['previous','stop','pause','play','fast','reset','playred'],
    'toolbar_object': ['folder','save','saveas','delete','down','left','right'],
    'object_icons': ['hand','apparatus','shield','bone','book','shirt','chest','door','leaf','bulb','pick','cube','probe','hammer','house','sword','person','paw','question','spark','leaf','potion','question','face'],
}
SINGLES = {
    'window_main': ('game',32,32), 'window_cell_view': ('search',32,32), 'window_plane': ('plane',32,32),
    'window_render': ('camera',32,32), 'window_face': ('face',32,32), 'window_weapon': ('sword',32,32),
    'window_npc': ('person',32,32), 'window_armor': ('shield',32,32), 'window_creature': ('paw',32,32),
    'window_lockpick': ('pick',32,32), 'window_alchemy': ('potion',32,32), 'window_activator': ('hand',32,32),
    'window_speaker': ('speaker',32,32), 'datafiles_check_checked': ('checked',16,16), 'datafiles_check_unchecked': ('unchecked',16,16),
    'scenegraph_103': ('world',16,15), 'scenegraph_104': ('cube',16,15), 'scenegraph_105': ('camera',16,15),
    'scenegraph_106': ('delete',16,15), 'scenegraph_107': ('folder',16,16), 'scenegraph_108': ('camera',16,16),
    'scenegraph_109': ('search',16,15), 'scenegraph_113': ('search',16,15),
}


def main():
    resources=[];catalog=[]
    for theme, palette in PALETTES.items():
        folder=ROOT/theme;folder.mkdir(parents=True,exist_ok=True)
        svg_folder=ROOT/'svg'/theme;svg_folder.mkdir(parents=True,exist_ok=True)
        for name, cells in STRIPS.items():
            strip=Image.new('RGBA',(len(cells)*16,16))
            svg=[]
            for i,cell in enumerate(cells):
                c=Canvas(palette);glyph(c,cell);strip.paste(c.finish(),(i*16,0))
                svg.append(f'<g transform="translate({i*24},0)">{"".join(c.svg)}</g>')
            strip.save(folder/(name+'.png'))
            (svg_folder/(name+'.svg')).write_text(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {24*len(cells)} 24">'+''.join(svg)+'</svg>\n')
            catalog.append(dict(theme=theme,name=name,width=strip.width,height=strip.height))
        for name,(shape,w,h) in SINGLES.items():
            c=Canvas(palette,w,h);glyph(c,shape);c.finish().save(folder/(name+'.png'))
            (svg_folder/(name+'.svg')).write_text(c.document())
            catalog.append(dict(theme=theme,name=name,width=w,height=h))
        for name in sorted([*STRIPS,*SINGLES]):
            resources.append(f'"CSSE_ICON_{theme.upper()}_{name.upper()}" RCDATA "res\\\\icons\\\\{theme}\\\\{name}.png"')
    (ROOT/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
    (ROOT.parent/'icons.rc2').write_text('// Generated by scripts/generate-icons.py. PNG bytes are linked into CSSE.dll.\n'+'\n'.join(resources)+'\n')
    print(f'Generated {len(catalog)} embedded PNG resources and matching SVG sources.')


if __name__=='__main__':main()
