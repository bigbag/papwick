#include <algorithm>

#include "Golf.h"

namespace golf {

namespace {

constexpr Shape circle(float x, float y, float r) { return {Shape::Circle, x, y, 0, 0, r, 0}; }
constexpr Shape capsule(float x0, float y0, float x1, float y1, float r) {
  return {Shape::Capsule, x0, y0, x1, y1, r, 0};
}
constexpr Shape box(float x0, float y0, float x1, float y1, float r) { return {Shape::Box, x0, y0, x1, y1, r, 0}; }
constexpr Bump hill(float x, float y, float r, float h) { return {Bump::Hill, x, y, 0, 0, r, h}; }
constexpr Bump ridge(float x0, float y0, float x1, float y1, float w, float h) {
  return {Bump::Ridge, x0, y0, x1, y1, w, h};
}
constexpr Bump ring(float x, float y, float radius, float w, float h) { return {Bump::Ring, x, y, radius, 0, w, h}; }
constexpr Bump ripples(float x0, float y0, float x1, float y1, float wave, float h) {
  return {Bump::Ripples, x0, y0, x1, y1, wave, h};
}
constexpr Bump rise(float x0, float y0, float x1, float y1, float h) { return {Bump::Slope, x0, y0, x1, y1, 0, h}; }
constexpr Deco tree(float x, float y, float r) { return {Deco::Tree, x, y, r}; }
constexpr Deco flowers(float x, float y, float r) { return {Deco::Flowers, x, y, r}; }
constexpr Deco stone(float x, float y, float r) { return {Deco::Stone, x, y, r}; }
constexpr Deco lantern(float x, float y) { return {Deco::Lantern, x, y, 5}; }

// 1. A wide S lane from the lower left to the upper right. The ground falls gently to the hole.
constexpr Course firstRoll() {
  Course c;
  c.theme = Theme::Meadow;
  c.green.add(capsule(128, 372, 158, 300, 32));
  c.green.add(capsule(158, 300, 290, 262, 32));
  c.green.add(capsule(290, 262, 330, 158, 32));
  c.green.add(circle(334, 146, 38));
  c.bumps.add(rise(128, 372, 334, 146, -18));
  c.bumps.add(hill(222, 282, 26, 7));
  c.teeX = 132;
  c.teeY = 362;
  c.holeX = 338;
  c.holeY = 142;
  c.parMs = 5500;
  for (const Deco& d : {tree(92, 222, 30), tree(178, 150, 24), tree(404, 296, 30), tree(300, 378, 26),
                        tree(400, 196, 17), tree(232, 108, 15), flowers(236, 352, 26), flowers(96, 300, 14),
                        flowers(372, 380, 16), stone(262, 420, 9), stone(338, 330, 8), stone(70, 330, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 2. A wide lane with a round hill in the middle. Go around the hill, or over it with speed.
constexpr Course theHill() {
  Course c;
  c.theme = Theme::Meadow;
  c.green.add(box(150, 72, 316, 424, 46));
  c.bumps.add(hill(233, 248, 48, 30));
  c.bumps.add(hill(292, 366, 22, 8));
  c.bumps.add(hill(176, 136, 22, -7));
  c.sand.add(circle(286, 118, 17));
  c.teeX = 233;
  c.teeY = 396;
  c.holeX = 226;
  c.holeY = 108;
  c.parMs = 4500;
  for (const Deco& d : {tree(100, 150, 32), tree(90, 282, 26), tree(112, 392, 26), tree(368, 142, 30),
                        tree(380, 272, 26), tree(356, 390, 28), flowers(66, 216, 13), flowers(398, 332, 13),
                        flowers(118, 222, 12), stone(352, 208, 8), stone(124, 334, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 3. An L lane: up on the left, then right along the top, then down to the hole. Sand in the first corner. A bump
// across the top lane stops a slow ball.
constexpr Course dogleg() {
  Course c;
  c.theme = Theme::Meadow;
  c.green.add(capsule(126, 372, 126, 128, 32));
  c.green.add(capsule(126, 128, 352, 128, 32));
  c.green.add(capsule(352, 128, 352, 256, 32));
  c.sand.add(circle(112, 114, 22));
  c.bumps.add(rise(126, 128, 352, 128, -10));
  c.bumps.add(rise(352, 128, 352, 256, -8));
  c.bumps.add(ridge(240, 96, 240, 160, 10, 6));
  c.teeX = 126;
  c.teeY = 358;
  c.holeX = 352;
  c.holeY = 246;
  c.parMs = 7500;
  for (const Deco& d :
       {tree(252, 262, 46), tree(206, 334, 26), tree(304, 352, 30), tree(410, 330, 22), flowers(186, 214, 20),
        flowers(256, 196, 12), flowers(64, 250, 12), stone(420, 198, 8), stone(232, 420, 8), stone(166, 400, 6)}) {
    c.decos.add(d);
  }
  return c;
}

// 4. A round green with a pond. The narrow bridge across the pond is short, but the ground pulls to the right.
constexpr Course thePond() {
  Course c;
  c.theme = Theme::Coast;
  c.green.add(circle(233, 250, 172));
  c.water.add(circle(233, 250, 82));
  c.bridges.add(capsule(233, 322, 233, 178, 13));
  c.sand.add(circle(112, 188, 22));
  c.bumps.add(hill(262, 250, 80, -20));
  c.teeX = 233;
  c.teeY = 394;
  c.holeX = 233;
  c.holeY = 110;
  c.parMs = 4000;
  for (const Deco& d : {tree(81, 374, 17), tree(385, 374, 17), tree(388, 112, 15), tree(78, 116, 14), stone(48, 246, 7),
                        stone(420, 250, 8), stone(233, 440, 6), flowers(53, 302, 9), flowers(411, 190, 9)}) {
    c.decos.add(d);
  }
  return c;
}

// 5. A long lane over dunes: waves across the lane, and the ground leans to two bunkers on the left.
constexpr Course dunes() {
  Course c;
  c.theme = Theme::Coast;
  c.green.add(capsule(326, 364, 136, 126, 42));
  c.bumps.add(ripples(306, 340, 154, 152, 48, 3));
  c.bumps.add(rise(265, 218, 197, 272, -10));
  c.sand.add(circle(238, 301, 18));
  c.sand.add(circle(178, 226, 18));
  c.teeX = 320;
  c.teeY = 356;
  c.holeX = 142;
  c.holeY = 134;
  c.parMs = 4000;
  for (const Deco& d :
       {tree(374, 222, 26), tree(354, 156, 20), tree(94, 318, 28), tree(150, 398, 20), tree(290, 92, 14),
        stone(64, 228, 9), stone(386, 300, 7), flowers(262, 420, 15), flowers(56, 166, 11), flowers(204, 64, 9)}) {
    c.decos.add(d);
  }
  return c;
}

// 6. The hole is in a crater on a ring hill. Climb the rim with speed, then stop in the crater.
constexpr Course crater() {
  Course c;
  c.theme = Theme::Coast;
  c.green.add(circle(233, 250, 168));
  c.bumps.add(ring(233, 250, 50, 18, 22));
  c.bumps.add(hill(233, 250, 22, -6));
  c.bumps.add(hill(130, 172, 30, 14));
  c.bumps.add(hill(340, 320, 32, 14));
  c.bumps.add(hill(332, 162, 24, -10));
  c.sand.add(circle(148, 330, 22));
  c.teeX = 233;
  c.teeY = 392;
  c.holeX = 233;
  c.holeY = 250;
  c.parMs = 3000;
  for (const Deco& d : {tree(80, 380, 16), tree(388, 384, 16), tree(392, 108, 14), tree(76, 112, 14), stone(50, 250, 8),
                        stone(418, 246, 7), flowers(233, 66, 8), flowers(233, 436, 8)}) {
    c.decos.add(d);
  }
  return c;
}

// 7. A river crosses the green. A narrow bridge without walls leads to the far bank. The banks fall to the water.
constexpr Course theBridge() {
  Course c;
  c.theme = Theme::Evening;
  c.green.add(box(82, 86, 384, 404, 56));
  c.water.add(capsule(30, 300, 436, 184, 30));
  c.bridges.add(capsule(219, 195, 247, 295, 11));
  c.bumps.add(ridge(30, 300, 436, 184, 58, -12));
  c.teeX = 300;
  c.teeY = 372;
  c.holeX = 166;
  c.holeY = 140;
  c.parMs = 4500;
  for (const Deco& d : {lantern(56, 240), lantern(410, 250), lantern(160, 70), lantern(306, 430), tree(54, 142, 20),
                        tree(410, 300, 20), flowers(232, 64, 10)}) {
    c.decos.add(d);
  }
  return c;
}

// 8. A turning bar stands in the lane. It blocks the lane when it is across. Pass when it points along the lane.
constexpr Course windmill() {
  Course c;
  c.theme = Theme::Evening;
  c.green.add(capsule(233, 388, 233, 150, 52));
  c.green.add(circle(233, 124, 62));
  c.blocks.add(circle(233, 256, 8));
  c.mill = {233, 256, 46, 1.7f};
  c.bumps.add(hill(233, 330, 28, -6));
  c.bumps.add(hill(233, 120, 50, -6));
  c.teeX = 233;
  c.teeY = 374;
  c.holeX = 233;
  c.holeY = 112;
  c.parMs = 7000;
  for (const Deco& d :
       {lantern(150, 300), lantern(316, 300), lantern(150, 206), lantern(316, 206), tree(94, 240, 28),
        tree(372, 240, 28), tree(120, 380, 22), tree(346, 390, 22), flowers(140, 104, 16), flowers(326, 104, 16)}) {
    c.decos.add(d);
  }
  return c;
}

// 9. A spiral lane from the edge to the center. The ground falls to the center. Sand around the hole.
constexpr Course spiral() {
  Course c;
  c.theme = Theme::Evening;
  c.green.add(circle(233, 250, 190));
  c.blocks.add({Shape::Spiral, 233, 250, 46, 54, 3, 2});
  c.sand.add(circle(233, 250, 28));
  c.bumps.add(hill(233, 250, 150, -30));
  c.teeX = 372;
  c.teeY = 290;
  c.holeX = 233;
  c.holeY = 250;
  c.parMs = 14500;
  for (const Deco& d : {lantern(70, 110), lantern(396, 110), lantern(92, 394), lantern(374, 394)}) c.decos.add(d);
  return c;
}

// 10. Three ramps lead up to the hole. A slow ball rolls back down a ramp.
constexpr Course terraces() {
  Course c;
  c.theme = Theme::Autumn;
  c.green.add(capsule(180, 392, 250, 250, 38));
  c.green.add(capsule(250, 250, 200, 110, 38));
  c.bumps.add(rise(211, 329, 229, 293, 8));
  c.bumps.add(rise(243, 231, 230, 194, 8));
  c.bumps.add(rise(218, 161, 208, 132, 8));
  c.sand.add(circle(276, 252, 14));
  c.teeX = 182;
  c.teeY = 380;
  c.holeX = 202;
  c.holeY = 114;
  c.parMs = 4500;
  for (const Deco& d : {tree(321, 357, 34), tree(122, 289, 30), tree(115, 126, 28), tree(409, 271, 26),
                        tree(74, 203, 24), tree(376, 129, 22), tree(371, 208, 20), tree(94, 341, 18),
                        tree(134, 227, 16), tree(111, 394, 15), flowers(280, 140, 16), flowers(332, 65, 14),
                        flowers(320, 157, 12), stone(314, 410, 9), stone(70, 160, 8), stone(307, 220, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 11. Two big hills block the way. Cross the saddle between them with speed.
constexpr Course twinHills() {
  Course c;
  c.theme = Theme::Autumn;
  c.green.add(box(120, 70, 346, 420, 50));
  c.bumps.add(hill(168, 245, 58, 24));
  c.bumps.add(hill(298, 245, 58, 24));
  c.bumps.add(hill(233, 116, 26, -5));
  c.sand.add(circle(152, 112, 18));
  c.sand.add(circle(314, 112, 18));
  c.teeX = 233;
  c.teeY = 394;
  c.holeX = 233;
  c.holeY = 112;
  c.parMs = 4000;
  for (const Deco& d : {tree(399, 240, 34), tree(387, 139, 30), tree(72, 242, 28), tree(52, 303, 26),
                        tree(400, 332, 24), tree(71, 186, 22), tree(100, 87, 20), tree(174, 38, 18), tree(314, 49, 16),
                        tree(78, 123, 15), flowers(267, 32, 16), flowers(83, 348, 14), flowers(422, 291, 12),
                        stone(261, 444, 9), stone(23, 264, 8), stone(44, 138, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 12. The hole is on a wooden deck in a lake. A walkway leads to the deck from the right shore.
constexpr Course island() {
  Course c;
  c.theme = Theme::Autumn;
  c.green.add(circle(233, 250, 186));
  c.water.add(circle(233, 236, 108));
  c.bridges.add(capsule(218, 236, 248, 236, 30));
  c.bridges.add(capsule(345, 236, 248, 236, 10));
  c.bumps.add(hill(233, 236, 110, -14));
  c.sand.add(circle(118, 140, 18));
  c.sand.add(circle(352, 372, 16));
  c.teeX = 233;
  c.teeY = 412;
  c.holeX = 233;
  c.holeY = 236;
  c.parMs = 6000;
  for (const Deco& d : {tree(182, 41, 22), tree(358, 76, 20), tree(130, 62, 18), tree(284, 39, 16), tree(423, 152, 15),
                        flowers(437, 205, 16), flowers(62, 135, 14), flowers(235, 32, 12), stone(41, 189, 9),
                        stone(430, 294, 8), stone(29, 249, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 13. A ring road around a wood. Bumpers on the left way, sand on the right way.
constexpr Course ringRoad() {
  Course c;
  c.theme = Theme::Autumn;
  c.green.add(circle(233, 250, 186));
  c.blocks.add(circle(233, 250, 108));
  c.blocks.add(circle(84, 214, 12));
  c.blocks.add(circle(90, 296, 12));
  c.sand.add(capsule(382, 200, 382, 300, 24));
  c.bumps.add(hill(233, 102, 30, -5));
  c.teeX = 233;
  c.teeY = 400;
  c.holeX = 233;
  c.holeY = 102;
  c.parMs = 7000;
  for (const Deco& d :
       {stone(84, 214, 6), stone(90, 296, 6), tree(191, 253, 34), tree(303, 237, 30), tree(269, 304, 28),
        tree(224, 189, 26), tree(210, 316, 24), tree(241, 34, 22), tree(160, 48, 20), tree(62, 125, 18),
        tree(388, 107, 16), tree(171, 192, 15), flowers(348, 64, 16), flowers(301, 51, 14), flowers(247, 258, 12),
        stone(420, 327, 9), stone(290, 184, 8), stone(121, 79, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 14. Four round bumpers guard the hole. The ground rises to the hole.
constexpr Course pinball() {
  Course c;
  c.theme = Theme::Autumn;
  c.green.add(circle(233, 250, 180));
  c.blocks.add(circle(233, 172, 16));
  c.blocks.add(circle(172, 238, 16));
  c.blocks.add(circle(294, 238, 16));
  c.blocks.add(circle(233, 304, 16));
  c.bumps.add(rise(233, 400, 233, 96, 14));
  c.bumps.add(hill(233, 112, 24, -5));
  c.sand.add(circle(126, 150, 18));
  c.sand.add(circle(340, 150, 18));
  c.teeX = 233;
  c.teeY = 402;
  c.holeX = 233;
  c.holeY = 110;
  c.parMs = 5500;
  for (const Deco& d : {stone(233, 172, 10), stone(172, 238, 10), stone(294, 238, 10), stone(233, 304, 10),
                        tree(276, 42, 26), tree(122, 71, 24), tree(169, 52, 22), tree(76, 111, 20), tree(388, 110, 18),
                        tree(31, 239, 16), tree(330, 56, 15), flowers(436, 251, 16), flowers(41, 179, 14),
                        flowers(346, 86, 12), stone(442, 192, 9), stone(410, 145, 8), stone(434, 220, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 15. Gates from both sides. The ground falls to the hole, so the ball gets fast.
constexpr Course slalom() {
  Course c;
  c.theme = Theme::Autumn;
  c.green.add(box(140, 60, 326, 420, 40));
  c.blocks.add(capsule(120, 336, 240, 336, 6));
  c.blocks.add(capsule(346, 266, 226, 266, 6));
  c.blocks.add(capsule(120, 196, 240, 196, 6));
  c.blocks.add(capsule(346, 126, 226, 126, 6));
  c.bumps.add(rise(233, 400, 233, 80, -12));
  c.teeX = 292;
  c.teeY = 392;
  c.holeX = 290;
  c.holeY = 88;
  c.parMs = 9500;
  for (const Deco& d : {tree(408, 200, 34), tree(366, 346, 30), tree(96, 369, 28), tree(90, 276, 26),
                        tree(388, 286, 24), tree(377, 114, 22), tree(85, 157, 20), tree(122, 62, 18),
                        tree(107, 219, 16), tree(83, 115, 15), flowers(353, 232, 16), flowers(318, 46, 14),
                        flowers(39, 158, 12), stone(106, 323, 9), stone(345, 273, 8), stone(354, 84, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 16. Up the left lane, over the crest at the top and down the right lane. Snowdrifts line the top.
constexpr Course hairpin() {
  Course c;
  c.theme = Theme::Winter;
  c.green.add(box(100, 70, 366, 410, 60));
  c.blocks.add(box(205, 166, 261, 440, 20));
  c.bumps.add(hill(233, 104, 96, 14));
  c.bumps.add(hill(316, 376, 26, -6));
  c.sand.add(capsule(150, 82, 316, 82, 14));
  c.teeX = 152;
  c.teeY = 376;
  c.holeX = 316;
  c.holeY = 376;
  c.parMs = 8000;
  for (const Deco& d : {tree(55, 195, 34), tree(414, 295, 30), tree(406, 178, 28), tree(41, 262, 26), tree(230, 38, 24),
                        tree(397, 237, 22), tree(234, 349, 20), tree(394, 110, 18), tree(236, 417, 16),
                        tree(181, 432, 15), flowers(74, 318, 16), flowers(93, 84, 14), flowers(234, 204, 12),
                        stone(233, 311, 9), stone(160, 35, 8), stone(393, 370, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 17. A ski slope with moguls. Each bump throws the ball to the side.
constexpr Course moguls() {
  Course c;
  c.theme = Theme::Winter;
  c.green.add(capsule(150, 350, 316, 116, 62));
  for (const Bump& b : {hill(208, 321, 20, 9), hill(184, 251, 20, 9), hill(233, 286, 20, 9), hill(258, 250, 20, 9),
                        hill(209, 216, 20, 9), hill(282, 215, 20, 9), hill(233, 181, 20, 9), hill(307, 180, 20, 9)}) {
    c.bumps.add(b);
  }
  c.teeX = 156;
  c.teeY = 342;
  c.holeX = 310;
  c.holeY = 124;
  c.parMs = 4000;
  for (const Deco& d : {tree(145, 152, 34), tree(43, 211, 30), tree(216, 422, 28), tree(335, 353, 26),
                        tree(183, 84, 24), tree(429, 262, 22), tree(72, 307, 20), tree(372, 289, 18),
                        tree(289, 412, 16), tree(274, 359, 15), flowers(398, 156, 16), flowers(87, 235, 14),
                        flowers(317, 298, 12), stone(397, 105, 9), stone(132, 56, 8), stone(130, 210, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 18. Two frozen ponds leave a narrow strip of land. The ground leans to the right pond.
constexpr Course isthmus() {
  Course c;
  c.theme = Theme::Winter;
  c.green.add(circle(233, 245, 180));
  c.water.add(circle(140, 236, 78));
  c.water.add(circle(326, 236, 78));
  c.bumps.add(rise(150, 236, 316, 236, -12));
  c.sand.add(circle(178, 104, 16));
  c.sand.add(circle(288, 104, 16));
  c.teeX = 233;
  c.teeY = 400;
  c.holeX = 233;
  c.holeY = 96;
  c.parMs = 4500;
  for (const Deco& d : {tree(280, 41, 22), tree(198, 35, 20), tree(120, 65, 18), tree(34, 202, 16), tree(60, 120, 15),
                        flowers(342, 70, 16), flowers(433, 240, 14), flowers(432, 290, 12), stone(408, 343, 9),
                        stone(86, 92, 8), stone(79, 377, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 19. A snake lane with three hairpin bends. Snowdrifts wait in the bends.
constexpr Course snake() {
  Course c;
  c.theme = Theme::Winter;
  c.green.add(capsule(150, 384, 326, 340, 28));
  c.green.add(capsule(326, 340, 140, 262, 28));
  c.green.add(capsule(140, 262, 326, 180, 28));
  c.green.add(capsule(326, 180, 160, 100, 28));
  c.sand.add(circle(342, 344, 15));
  c.sand.add(circle(124, 262, 15));
  c.sand.add(circle(342, 180, 15));
  c.bumps.add(rise(150, 384, 160, 100, -10));
  c.teeX = 158;
  c.teeY = 382;
  c.holeX = 168;
  c.holeY = 102;
  c.parMs = 8500;
  for (const Deco& d : {tree(73, 276, 34), tree(213, 53, 30), tree(322, 98, 28), tree(341, 258, 26), tree(88, 202, 24),
                        tree(406, 283, 22), tree(67, 127, 20), tree(412, 189, 18), tree(122, 316, 16),
                        tree(42, 223, 15), flowers(406, 235, 16), flowers(132, 191, 14), flowers(81, 351, 12),
                        stone(438, 223, 9), stone(353, 136, 8), stone(170, 324, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 20. Two walls form a funnel with a narrow gap. The way around the walls leads through snowdrifts.
constexpr Course funnel() {
  Course c;
  c.theme = Theme::Winter;
  c.green.add(circle(233, 245, 178));
  c.blocks.add(capsule(108, 170, 210, 300, 7));
  c.blocks.add(capsule(358, 170, 256, 300, 7));
  c.sand.add(circle(118, 300, 26));
  c.sand.add(circle(348, 300, 26));
  c.bumps.add(rise(233, 96, 233, 380, -16));
  c.bumps.add(hill(233, 372, 24, -5));
  c.teeX = 233;
  c.teeY = 100;
  c.holeX = 233;
  c.holeY = 372;
  c.parMs = 4000;
  for (const Deco& d : {tree(230, 36, 24), tree(102, 86, 22), tree(357, 78, 20), tree(402, 129, 18), tree(288, 49, 16),
                        tree(429, 177, 15), flowers(47, 162, 16), flowers(438, 265, 14), flowers(91, 385, 12),
                        stone(385, 382, 9), stone(33, 245, 8), stone(72, 128, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 21. A wall crosses the green. A turning bar guards the only gate.
constexpr Course gate() {
  Course c;
  c.theme = Theme::Winter;
  c.green.add(circle(233, 250, 182));
  c.blocks.add(capsule(40, 250, 192, 250, 8));
  c.blocks.add(capsule(274, 250, 426, 250, 8));
  c.blocks.add(circle(233, 250, 6));
  c.mill = {233, 250, 24, 2.0f};
  c.bumps.add(hill(233, 118, 40, -6));
  c.teeX = 233;
  c.teeY = 404;
  c.holeX = 233;
  c.holeY = 116;
  c.parMs = 8500;
  for (const Deco& d : {tree(178, 45, 24), tree(371, 90, 22), tree(412, 142, 20), tree(429, 186, 18), tree(333, 53, 16),
                        tree(278, 37, 15), flowers(437, 252, 16), flowers(70, 128, 14), flowers(237, 31, 12),
                        stone(87, 95, 9), stone(42, 186, 8), stone(427, 307, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 22. A maze of walls. The gaps are at the right, the left and the right again.
constexpr Course labyrinth() {
  Course c;
  c.theme = Theme::Evening;
  c.green.add(box(96, 76, 370, 400, 40));
  c.blocks.add(capsule(80, 322, 296, 322, 6));
  c.blocks.add(capsule(386, 244, 170, 244, 6));
  c.blocks.add(capsule(80, 166, 296, 166, 6));
  c.blocks.add(capsule(240, 60, 240, 120, 6));
  c.sand.add(circle(233, 283, 14));
  c.bumps.add(hill(233, 205, 20, -5));
  c.teeX = 136;
  c.teeY = 372;
  c.holeX = 150;
  c.holeY = 112;
  c.parMs = 13000;
  for (const Deco& d :
       {lantern(415, 155), lantern(431, 309), lantern(34, 170), lantern(386, 210), tree(57, 273, 30), tree(243, 38, 24),
        tree(416, 218, 20), tree(393, 323, 16), flowers(423, 274, 16), flowers(55, 151, 14), flowers(54, 332, 12),
        stone(393, 128, 9), stone(384, 249, 8), stone(137, 43, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 23. Two canals cross the green. Their bridges are at opposite ends. The strip between them is a crest.
constexpr Course canals() {
  Course c;
  c.theme = Theme::Coast;
  c.green.add(circle(233, 245, 180));
  c.water.add(capsule(20, 186, 446, 186, 14));
  c.water.add(capsule(20, 300, 446, 300, 14));
  c.bridges.add(capsule(316, 166, 316, 206, 12));
  c.bridges.add(capsule(150, 280, 150, 320, 12));
  c.bumps.add(ridge(20, 243, 446, 243, 28, 7));
  c.sand.add(circle(170, 116, 16));
  c.teeX = 233;
  c.teeY = 400;
  c.holeX = 233;
  c.holeY = 104;
  c.parMs = 8000;
  for (const Deco& d : {tree(185, 39, 22), tree(99, 83, 20), tree(261, 34, 18), tree(31, 201, 16), tree(140, 58, 15),
                        flowers(430, 174, 16), flowers(39, 294, 14), flowers(69, 131, 12), stone(438, 249, 9),
                        stone(341, 81, 8), stone(413, 342, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 24. The hole is on top of a hill in a ring of low ground. Stop the ball on the summit.
constexpr Course summit() {
  Course c;
  c.theme = Theme::Meadow;
  c.green.add(circle(233, 245, 180));
  c.bumps.add(hill(233, 245, 66, 20));
  c.bumps.add(ring(233, 245, 118, 20, -8));
  c.blocks.add(circle(166, 168, 14));
  c.blocks.add(circle(306, 178, 14));
  c.blocks.add(circle(248, 336, 14));
  c.sand.add(circle(104, 300, 18));
  c.sand.add(circle(360, 196, 18));
  c.teeX = 233;
  c.teeY = 404;
  c.holeX = 233;
  c.holeY = 245;
  c.parMs = 4500;
  for (const Deco& d :
       {stone(166, 168, 8), stone(306, 178, 8), stone(248, 336, 8), tree(232, 35, 22), tree(170, 45, 20),
        tree(316, 54, 18), tree(65, 121, 16), tree(354, 76, 15), flowers(434, 205, 16), flowers(126, 74, 14),
        flowers(399, 133, 12), stone(275, 50, 9), stone(41, 324, 8), stone(425, 305, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 25. A turning bar in the lane, then two ponds beside the hole.
constexpr Course millAlley() {
  Course c;
  c.theme = Theme::Meadow;
  c.green.add(capsule(233, 390, 233, 190, 44));
  c.green.add(circle(233, 140, 84));
  c.blocks.add(circle(233, 290, 7));
  c.mill = {233, 290, 38, -2.2f};
  c.water.add(circle(172, 130, 26));
  c.water.add(circle(294, 130, 26));
  c.bumps.add(hill(233, 100, 30, -5));
  c.teeX = 233;
  c.teeY = 376;
  c.holeX = 233;
  c.holeY = 100;
  c.parMs = 5000;
  for (const Deco& d : {tree(327, 288, 34), tree(49, 269, 30), tree(103, 134, 28), tree(117, 377, 26),
                        tree(365, 238, 24), tree(129, 249, 22), tree(381, 313, 20), tree(351, 124, 18),
                        tree(329, 197, 16), tree(85, 318, 15), flowers(428, 173, 16), flowers(314, 350, 14),
                        flowers(127, 77, 12), stone(92, 197, 9), stone(32, 204, 8), stone(335, 52, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 26. Waves roll across the green. The sea waits on both sides.
constexpr Course waves() {
  Course c;
  c.theme = Theme::Coast;
  c.green.add(circle(233, 245, 180));
  c.water.add(circle(40, 245, 90));
  c.water.add(circle(426, 245, 90));
  c.bumps.add(ripples(233, 410, 233, 80, 70, 4));
  c.bumps.add(hill(233, 96, 26, -5));
  c.sand.add(circle(233, 250, 20));
  c.teeX = 233;
  c.teeY = 404;
  c.holeX = 233;
  c.holeY = 96;
  c.parMs = 4500;
  for (const Deco& d : {tree(195, 40, 22), tree(415, 146, 20), tree(349, 69, 18), tree(67, 121, 16), tree(112, 75, 15),
                        flowers(259, 38, 16), flowers(313, 54, 14), flowers(405, 352, 12), stone(45, 190, 9),
                        stone(28, 241, 8), stone(302, 428, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// 27. The last hole: a deck in a pond, and the ground falls to the pond. A wall closes the left way. A turning bar
// guards the gate on the right way. The bridge to the deck starts at the far shore.
constexpr Course finale() {
  Course c;
  c.theme = Theme::Evening;
  c.green.add(circle(233, 245, 184));
  c.water.add(circle(233, 236, 104));
  c.bridges.add(capsule(220, 230, 246, 230, 26));
  c.bridges.add(capsule(233, 128, 233, 222, 10));
  c.blocks.add(capsule(36, 236, 134, 236, 7));
  c.blocks.add(capsule(330, 236, 340, 236, 7));
  c.blocks.add(circle(380, 236, 6));
  c.mill = {380, 236, 24, 1.8f};
  c.bumps.add(hill(233, 236, 100, -14));
  c.sand.add(circle(326, 382, 16));
  c.teeX = 233;
  c.teeY = 404;
  c.holeX = 233;
  c.holeY = 230;
  c.parMs = 9000;
  for (const Deco& d : {lantern(441, 186), lantern(210, 22), lantern(356, 66), lantern(30, 201), tree(243, 34, 20),
                        tree(140, 56, 16), flowers(286, 35, 16), flowers(29, 265, 14), flowers(96, 74, 12),
                        stone(55, 156, 9), stone(404, 115, 8), stone(54, 349, 7)}) {
    c.decos.add(d);
  }
  return c;
}

// The courses are constant data, so they stay in flash and use no RAM.
constexpr Course kCourses[] = {firstRoll(), theHill(), dogleg(),   thePond(),   dunes(),  crater(),   theBridge(),
                               windmill(),  spiral(),  terraces(), twinHills(), island(), ringRoad(), pinball(),
                               slalom(),    hairpin(), moguls(),   isthmus(),   snake(),  funnel(),   gate(),
                               labyrinth(), canals(),  summit(),   millAlley(), waves(),  finale()};
static_assert(sizeof(kCourses) / sizeof(*kCourses) == kHoles, "one course for each hole");

}  // namespace

const Course& course(int index) { return kCourses[std::clamp(index, 0, kHoles - 1)]; }

}  // namespace golf
