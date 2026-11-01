"""Stained glass, ported from ps1-src worlds_a.rose_window / lancet (numpy only, smooth fields, no noise grain).
Returns top-down float RGBA; alpha 0 marks the original cut-out area."""
import math
import numpy as np


def _hx(s):
    s = s.lstrip('#')
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], float) / 255


def _shade(f, cols):
    pal = np.array([_hx(c) for c in cols])
    x = np.clip(f, 0, 1) * (len(pal) - 1)
    i = np.floor(x).astype(int)
    j = np.minimum(i + 1, len(pal) - 1)
    a = (x - i)[..., None]
    return pal[i] * (1 - a) + pal[j] * a


LEAD = np.array([6, 5, 12]) / 255


def rose_window(n=128):
    t = np.zeros((n, n, 4), np.float32)
    yy, xx = np.mgrid[0:n, 0:n]
    c = (n - 1) / 2
    d, a = np.hypot(xx - c, yy - c), np.arctan2(yy - c, xx - c)
    r = d / (n / 2)
    pals = (("#3a0612", "#a01428", "#ff4a4a", "#ffb0a0"), ("#06103a", "#1438b0", "#3a8cff", "#b0e0ff"),
            ("#3a2406", "#c88a14", "#ffd84a", "#fff8c0"), ("#20063a", "#6a1ab0", "#b45aff", "#ecc0ff"),
            ("#063a30", "#14a088", "#4affd0", "#c0fff0"))
    petals = 12
    sector = np.floor((a + math.pi) / (2 * math.pi) * petals).astype(int) % petals
    local = ((a + math.pi) / (2 * math.pi) * petals) % 1
    band = np.select([r < .16, r < .42, r < .72, r < .94], [0, 1, 2, 3], 4)
    idx = (band * 2 + sector * (band > 0) * ((band % 2) + 1)) % 5
    f = np.clip(.75 - r * .35 + .12 * np.sin(xx * .31) * np.sin(yy * .27), 0, 1)
    for i in range(5):
        s = _shade(f, pals[i])
        t[idx == i, :3] = s[idx == i]
    lead = (np.abs(local - .5) > .46) & (band > 0) & (band < 4)
    for b in (.16, .42, .72, .94):
        lead |= np.abs(r - b) < .018
    petal = np.abs(r - (.57 + .15 * np.cos((local - .5) * 2 * math.pi))) < .016
    t[lead | petal, :3] = LEAD
    t[..., 3] = (r <= .99).astype(np.float32)
    return t


def lancet(w=32, h=96, seed=4):
    t = np.zeros((h, w, 4), np.float32)
    rng = np.random.default_rng(seed)
    pts = rng.uniform(0, 1, (34, 2)) * np.array([w, h])
    yy, xx = np.mgrid[0:h, 0:w]
    d = np.hypot(xx[..., None] - pts[:, 0], yy[..., None] - pts[:, 1])
    order = np.argsort(d, -1)
    ids = order[..., 0]
    e = np.take_along_axis(d, order[..., 1:2], -1)[..., 0] - np.take_along_axis(d, order[..., :1], -1)[..., 0]
    pals = (("#06103a", "#1438b0", "#3a8cff"), ("#20063a", "#6a1ab0", "#b45aff"), ("#063a30", "#14a088", "#4affd0"), ("#3a2406", "#c88a14", "#ffd84a"))
    f = np.clip(.7 - yy / h * .3 + .1 * np.sin(xx * .7 + yy * .2), 0, 1)
    for i in range(4):
        s = _shade(f, pals[i])
        t[ids % 4 == i, :3] = s[ids % 4 == i]
    t[e < 1.2, :3] = LEAD
    arch = yy > (np.abs(xx - w / 2) / (w / 2)) ** .55 * 26
    t[(xx < 2) | (xx > w - 3) | (yy > h - 3), :3] = LEAD
    t[..., 3] = (arch | (yy > 26)).astype(np.float32)
    return t
