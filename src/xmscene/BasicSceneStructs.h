/*=============================================================================
XMOTO

This file is part of XMOTO.

XMOTO is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

XMOTO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XMOTO; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
=============================================================================*/

#ifndef __BASICSCENESTRUCTS_H__
#define __BASICSCENESTRUCTS_H__

/**
  An entity can have only one Speciality
*/
enum EntitySpeciality {
  ET_NONE = 1,
  ET_ISSTART = 2,
  ET_MAKEWIN = 3,
  ET_KILL = 4,
  ET_ISTOTAKE = 5,
  ET_PARTICLES_SOURCE = 6,
  ET_JOINT = 7,
  ET_CHECKPOINT = 8
};

/*===========================================================================
  Driving directions
  ===========================================================================*/
enum DriveDir { DD_RIGHT, DD_LEFT };

enum ZonePrimType { LZPT_BOX = 1 };

/* IMPORTANT: This structure must be kept as is, otherwise replays will
     be broken! */
/* Replays keep these raw (byte-swapped to little-endian), at the PC's
   layout: 44 bytes, the floats at 4-byte offsets. The 68k aligns a float to
   2 bytes, which would make it 42 and misread every PC replay, so on
   AmigaOS the first float is aligned as on the PC (checked below). */
#if defined(__amigaos__)
#define XM_PC_ALIGN4 __attribute__((aligned(4)))
#else
#define XM_PC_ALIGN4
#endif

struct SerializedBikeState {
  unsigned char cFlags; /* State flags */
  float fGameTime XM_PC_ALIGN4; /* Game time */
  float fFrameX, fFrameY; /* Frame position */
  float fMaxXDiff, fMaxYDiff; /* Addressing space around the frame */

  unsigned short nRearWheelRot; /* Encoded rear wheel matrix */
  unsigned short nFrontWheelRot; /* Encoded front wheel matrix */
  unsigned short nFrameRot; /* Encoded frame matrix */

  unsigned char cBikeEngineRPM; /* Maps to a float between 400 and 5000 */

  signed char cRearWheelX, cRearWheelY; /* Rear wheel position */
  signed char cFrontWheelX, cFrontWheelY; /* Front wheel position */
  signed char cElbowX, cElbowY; /* Elbow position */
  signed char cShoulderX, cShoulderY; /* Shoulder position */
  signed char cLowerBodyX, cLowerBodyY; /* Ass position */
  signed char cKneeX, cKneeY; /* Knee position */
};

#if defined(__amigaos__)
#include <cstddef>
static_assert(sizeof(SerializedBikeState) == 44, "replay state size");
static_assert(offsetof(SerializedBikeState, fGameTime) == 4, "replay layout");
static_assert(offsetof(SerializedBikeState, nRearWheelRot) == 24,
              "replay layout");
static_assert(offsetof(SerializedBikeState, cBikeEngineRPM) == 30,
              "replay layout");
static_assert(offsetof(SerializedBikeState, cKneeY) == 42, "replay layout");
#endif

/* controls */
enum PlayerControl {
  PC_BRAKE = 1,
  PC_THROTTLE = 2,
  PC_PULL = 3,
  PC_CHANGEDIR = 4
};
bool PlayerControl_isValid(PlayerControl p);

#endif /* __BASICSCENESTRUCTS_H__ */
