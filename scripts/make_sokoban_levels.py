#!/usr/bin/env python3
"""Generate Sokoban levels with a known push-optimal solution.

The script makes random rooms with walls. For each room, it searches back from the solved state, so the
player pulls the boxes. The state farthest from the solved state becomes the start. Thus each level is
solvable, and the search distance is the minimum number of pushes.

    python3 scripts/make_sokoban_levels.py 9 9 3 --density 0.26 --rooms 4000 --seed 40000

The output shows the best levels as entries for lib/SokobanCore/src/SokobanLevels.cpp, with their
solutions for test/unit/SokobanTest.cpp. Width and height include the outer walls. Keep them at 12 or
less, and keep 8 boxes or less (kMaxSide and kMaxBoxes in lib/SokobanCore/src/Sokoban.h).
"""

import argparse
import os
import random
from collections import deque
from multiprocessing import Pool

LETTERS = {}


def flood(grid, w, start, boxes):
    seen = {start}
    stack = [start]
    while stack:
        c = stack.pop()
        for d in (1, -1, w, -w):
            n = c + d
            if grid[n] != "#" and n not in boxes and n not in seen:
                seen.add(n)
                stack.append(n)
    return seen


def make_room(w, h, density, rng):
    grid = ["#"] * (w * h)
    inner = [y * w + x for y in range(1, h - 1) for x in range(1, w - 1)]
    for i in inner:
        grid[i] = " "
    for i in rng.sample(inner, int(len(inner) * density)):
        grid[i] = "#"
    # A floor cell with three or four wall neighbors is a dead end. Fill it.
    changed = True
    while changed:
        changed = False
        for i in inner:
            if grid[i] == " " and sum(grid[i + d] == "#" for d in (1, -1, w, -w)) >= 3:
                grid[i] = "#"
                changed = True
    floor = [i for i in inner if grid[i] == " "]
    if not floor:
        return None
    largest = max((flood(grid, w, f, set()) for f in floor), key=len)
    for i in inner:
        if i not in largest:
            grid[i] = "#"
    return grid


def state(grid, w, player, boxes):
    """The player can walk in its area, so the lowest cell of the area identifies the state."""
    return boxes, min(flood(grid, w, player, boxes))


def pull_distances(grid, w, goals):
    """Breadth-first search with pulls from all solved states. Returns the push distance of each state."""
    solved = frozenset(goals)
    dist = {}
    queue = deque()
    rest = {i for i, c in enumerate(grid) if c != "#"} - solved
    while rest:
        area = flood(grid, w, min(rest), solved)
        rest -= area
        dist[(solved, min(area))] = 0
        queue.append((solved, min(area)))
    while queue:
        boxes, player = queue.popleft()
        area = flood(grid, w, player, boxes)
        for b in boxes:
            for d in (1, -1, w, -w):
                p, t = b + d, b + 2 * d
                if p in area and grid[t] != "#" and t not in boxes:
                    s = state(grid, w, t, (boxes - {b}) | {p})
                    if s not in dist:
                        dist[s] = dist[(boxes, player)] + 1
                        queue.append(s)
    return dist


def walk(grid, w, src, dst, boxes):
    prev = {src: None}
    queue = deque([src])
    while queue:
        c = queue.popleft()
        for d in (1, -1, w, -w):
            n = c + d
            if grid[n] != "#" and n not in boxes and n not in prev:
                prev[n] = c
                queue.append(n)
    out = []
    while prev[dst] is not None:
        out.append(LETTERS[dst - prev[dst]])
        dst = prev[dst]
    return "".join(reversed(out))


def solve(grid, w, dist, boxes, player):
    """Follows the distances down to 0. Each push goes to a state that is one push nearer to the solution."""
    moves = []
    while dist[state(grid, w, player, boxes)] > 0:
        target = dist[state(grid, w, player, boxes)] - 1
        area = flood(grid, w, player, boxes)
        push = next(
            (b, d)
            for b in sorted(boxes)
            for d in (1, -1, w, -w)
            if b - d in area
            and grid[b + d] != "#"
            and b + d not in boxes
            and dist.get(state(grid, w, b, (boxes - {b}) | {b + d})) == target
        )
        b, d = push
        moves.append(walk(grid, w, player, b - d, boxes) + LETTERS[d])
        boxes, player = (boxes - {b}) | {b + d}, b
    return "".join(moves)


def replay(grid, w, boxes, player, moves):
    boxes = set(boxes)
    for m in moves:
        d = {v: k for k, v in LETTERS.items()}[m]
        player += d
        assert grid[player] != "#"
        if player in boxes:
            assert grid[player + d] != "#" and player + d not in boxes
            boxes.remove(player)
            boxes.add(player + d)
    return boxes


def level(args):
    w, h, count, density, seed = args
    rng = random.Random(seed)
    grid = make_room(w, h, density, rng)
    floor = [i for i, c in enumerate(grid or []) if c != "#"]
    if len(floor) < count * 3 + 4:
        return None
    goals = rng.sample(floor, count)
    dist = pull_distances(grid, w, goals)
    pushes = max(dist.values())
    boxes, player = next(s for s, v in dist.items() if v == pushes)
    moves = solve(grid, w, dist, boxes, player)
    assert replay(grid, w, boxes, player, moves) == set(goals)
    rows = []
    for y in range(h):
        row = ""
        for x in range(w):
            i = y * w + x
            goal, box = i in goals, i in boxes
            if grid[i] == "#":
                row += "#"
            elif i == player:
                row += "+" if goal else "@"
            else:
                row += "*" if goal and box else "$" if box else "." if goal else " "
        rows.append(row)
    # Cut the rows and columns that have only walls, but keep one wall around the room.
    ys = [y for y, row in enumerate(rows) if row.strip("#")]
    xs = [x for x in range(w) if any(row[x] != "#" for row in rows)]
    rows = [row[xs[0] - 1 : xs[-1] + 2] for row in rows[ys[0] - 1 : ys[-1] + 2]]
    return {"rows": rows, "pushes": pushes, "moves": moves, "seed": seed}


def init(w):
    LETTERS.update({1: "R", -1: "L", w: "D", -w: "U"})


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("width", type=int, help="Room width with the outer walls, 12 or less")
    parser.add_argument("height", type=int, help="Room height with the outer walls, 12 or less")
    parser.add_argument("boxes", type=int, help="Number of boxes, 8 or less")
    parser.add_argument("--density", type=float, default=0.25, help="Part of the inner cells that become walls")
    parser.add_argument("--rooms", type=int, default=1000, help="Number of random rooms to try")
    parser.add_argument("--seed", type=int, default=0, help="Seed of the first room")
    parser.add_argument("--best", type=int, default=5, help="Number of levels to show")
    args = parser.parse_args()
    if args.width > 12 or args.height > 12 or not 1 <= args.boxes <= 8:
        parser.error("width and height must be 12 or less, boxes 1 to 8")
    init(args.width)
    jobs = [(args.width, args.height, args.boxes, args.density, args.seed + k) for k in range(args.rooms)]
    with Pool(os.cpu_count(), initializer=init, initargs=(args.width,)) as pool:
        levels = [r for r in pool.map(level, jobs) if r]
    levels.sort(key=lambda r: (r["pushes"], len(r["moves"])), reverse=True)
    for r in levels[: args.best]:
        print(f"// seed {r['seed']}: {r['pushes']} pushes, {len(r['moves'])} moves")
        print(f"    {{{len(r['rows'][0])}, {len(r['rows'])},")
        print("\n".join(f'     "{row}"' for row in r["rows"]) + "},")
        print(f'      "{r["moves"]}",')
        print()


if __name__ == "__main__":
    main()
