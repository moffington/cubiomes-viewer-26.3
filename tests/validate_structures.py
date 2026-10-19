"""Independent random-spread placement check against extracted final 26.3 data.

Usage: python tests/validate_structures.py path/to/cubiomes.dll
This verifies candidates and vanilla configs, not terrain or biome viability.
Fixture source (version tag, downloaded 2026-10-18):
https://raw.githubusercontent.com/misode/mcmeta/26.3-summary/data/worldgen/structure_set/data.json
"""
import ctypes as C
import json
from pathlib import Path
import sys

class Config(C.Structure):
    _fields_ = [('salt', C.c_int32), ('regionSize', C.c_int8),
                ('chunkRange', C.c_int8), ('structType', C.c_uint8),
                ('dim', C.c_int8), ('rarity', C.c_float)]

class Pos(C.Structure):
    _fields_ = [('x', C.c_int), ('z', C.c_int)]

class JavaRandom:
    def __init__(self, seed):
        self.state = (seed ^ 0x5deece66d) & ((1 << 48) - 1)

    def bits(self, count):
        self.state = (self.state * 0x5deece66d + 11) & ((1 << 48) - 1)
        return self.state >> (48 - count)

    def next_int(self, bound):
        if bound & (bound - 1) == 0:
            return (bound * self.bits(31)) >> 31
        while True:
            bits = self.bits(31)
            val = bits % bound
            if bits - val + bound - 1 < (1 << 31):
                return val

library = C.CDLL(str(Path(sys.argv[1]).resolve()))
library.str2mc.argtypes = [C.c_char_p]
library.struct2str.restype = C.c_char_p
library.getStructureConfig.argtypes = [C.c_int, C.c_int, C.POINTER(Config)]
library.getStructurePos.argtypes = [C.c_int, C.c_int, C.c_uint64, C.c_int, C.c_int, C.POINTER(Pos)]
mc = library.str2mc(b'26.3')
types = {name.decode(): i for i in range(28) if (name := library.struct2str(i))}
fixtures = json.loads((Path(__file__).parent / 'fixtures/vanilla-26.3-structure-sets.json').read_text())
# These sets have a single attempt with no additional frequency/weight gating.
names = {
    'abandoned_camp': 'abandoned_camp', 'ancient_cities': 'ancient_city',
    'desert_pyramids': 'desert_pyramid', 'end_cities': 'end_city',
    'igloos': 'igloo', 'jungle_temples': 'jungle_pyramid',
    'ocean_monuments': 'monument', 'ocean_ruins': 'ocean_ruin',
    'ruined_portals': 'ruined_portal', 'shipwrecks': 'shipwreck',
    'swamp_huts': 'swamp_hut', 'trail_ruins': 'trail_ruins',
    'trial_chambers': 'trial_chambers', 'villages': 'village',
    'woodland_mansions': 'mansion',
}
count = 0
for key, name in names.items():
    placement = fixtures[key]['placement']
    stype = types[name]
    config = Config()
    assert library.getStructureConfig(stype, mc, C.byref(config)), name
    bound = placement['spacing'] - placement['separation']
    assert (config.salt, config.regionSize, config.chunkRange) == (placement['salt'], placement['spacing'], bound), name
    for seed in [0, 1, 12345, -3829811542736183482, -1]:
        for rx, rz in [(0, 0), (0, -1), (-3, 7), (17, -20)]:
            rng = JavaRandom(rx * 341873128712 + rz * 132897987541 + seed + placement['salt'])
            if placement.get('spread_type') == 'triangular':
                x = (rng.next_int(bound) + rng.next_int(bound)) // 2
                z = (rng.next_int(bound) + rng.next_int(bound)) // 2
            else:
                x, z = rng.next_int(bound), rng.next_int(bound)
            expected = ((rx * config.regionSize + x) * 16, (rz * config.regionSize + z) * 16)
            actual = Pos()
            exists = library.getStructurePos(stype, mc, seed, rx, rz, C.byref(actual))
            expected_exists = name != 'end_city' or expected[0]**2 + expected[1]**2 >= 1008**2
            assert bool(exists) == expected_exists, (name, seed, rx, rz)
            assert (actual.x, actual.z) == expected, (name, seed, rx, rz, expected, actual.x, actual.z)
            count += 1
    print('PASS', key, 'configuration and 20 candidate positions')
print(f'PASS {count} independent placement comparisons across {len(names)} structure types')

# Compare camp eligibility to the final release's individual biome tags.
# Source: same tag, data/tag/worldgen/biome/data.json.
tags = json.loads((Path(__file__).parent / 'fixtures/vanilla-26.3-camp-biomes.json').read_text())
allowed = {value.removeprefix('minecraft:') for tag in tags.values() for value in tag['values']}
library.biome2str.argtypes = [C.c_int, C.c_int]
library.biome2str.restype = C.c_char_p
library.isViableFeatureBiome.argtypes = [C.c_int, C.c_int, C.c_int]
for biome in range(256):
    raw = library.biome2str(mc, biome)
    name = raw.decode() if raw else ''
    assert bool(library.isViableFeatureBiome(mc, types['abandoned_camp'], biome)) == (name in allowed), (biome, name)
print(f'PASS camp biome eligibility: {len(allowed)} final-release biomes, all 256 IDs checked')
