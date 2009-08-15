/* roadmap_point.c - Manage the points that define tiger lines.
 *
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2008 Ehud Shabtai
 *
 *   This file is part of RoadMap.
 *
 *   RoadMap is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   RoadMap is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with RoadMap; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * SYNOPSYS:
 *
 *   int  roadmap_point_in_square (int square, int *first, int *last);
 *   void roadmap_point_position  (int point, RoadMapPosition *position);
 *
 * These functions are used to retrieve the points that make the lines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "roadmap.h"
#include "roadmap_dbread.h"
#include "roadmap_tile_model.h"
#include "roadmap_db_point.h"

#include "roadmap_square.h"
#include "roadmap_point.h"


typedef struct {

   char *type;

   RoadMapPoint *Point;
   int           PointCount;

   int *PointID;
   int  PointIDCount;

} RoadMapPointContext;

static RoadMapPointContext *RoadMapPointActive = NULL;


static void *roadmap_point_map (const roadmap_db_data_file *file) {

   RoadMapPointContext *context;

   context = malloc (sizeof(RoadMapPointContext));
   roadmap_check_allocated(context);
   context->type = "RoadMapPointContext";

   if (!roadmap_db_get_data (file,
                                  model__tile_point_data,
                                  sizeof (RoadMapPoint),
                                  (void**)&(context->Point),
                                  &(context->PointCount))) {
      roadmap_log (ROADMAP_FATAL, "invalid point/data structure");
   }

   if (!roadmap_db_get_data (file,
                                  model__tile_point_id,
                                  sizeof (int),
                                  (void**)&(context->PointID),
                                  &(context->PointIDCount))) {
      roadmap_log (ROADMAP_FATAL, "invalid point/id structure");
   }

   return context;
}

static void roadmap_point_activate (void *context) {

   RoadMapPointContext *point_context = (RoadMapPointContext *) context;

   if ((point_context != NULL) &&
       (strcmp (point_context->type, "RoadMapPointContext") != 0)) {
      roadmap_log (ROADMAP_FATAL, "cannot activate (invalid context type)");
   }
   RoadMapPointActive = point_context;
}

static void roadmap_point_unmap (void *context) {

   RoadMapPointContext *point_context = (RoadMapPointContext *) context;

   if (point_context == RoadMapPointActive) {
      RoadMapPointActive = NULL;
   }

   free (point_context);
}

roadmap_db_handler RoadMapPointHandler = {
   "point",
   roadmap_point_map,
   roadmap_point_activate,
   roadmap_point_unmap
};


void roadmap_point_position (int point, RoadMapPosition *position) {

   static int square = -2;
   static int scale_factor;
   static RoadMapPosition square_position;
   RoadMapPoint *Point;

   int point_id = point & POINT_REAL_MASK;
   int point_square = roadmap_square_active (); //(point >> 16) & 0xffff;

   //point_id += roadmap_square_first_point(point_square);

#ifdef DEBUG
   if (point_id < 0 || point_id >= RoadMapPointActive->PointCount) {
            roadmap_log (ROADMAP_FATAL, "invalid point index %d", point_id);
   }
#endif

   if (square != point_square) {

      square = point_square;
      roadmap_square_min (point_square, &square_position);
       scale_factor = roadmap_square_current_scale_factor ();
   }

   Point = RoadMapPointActive->Point + point_id;
   position->longitude = square_position.longitude + Point->longitude * scale_factor;
   position->latitude  = square_position.latitude  + Point->latitude * scale_factor;
}


int roadmap_point_db_id (int point) {

   int point_id = point & 0xffff;
   int point_square = roadmap_square_active (); //(point >> 16) & 0xffff;

   if ((RoadMapPointActive == NULL) ||
         (RoadMapPointActive->PointID == NULL)) {

      return -1;
   }

   point_id += roadmap_square_first_point(point_square);

   return RoadMapPointActive->PointID[point_id];
}


int roadmap_point_count (void) {

   if (RoadMapPointActive == NULL) return 0; /* No line. */

   return RoadMapPointActive->PointCount;
}

/*
int roadmap_point_square (int point) {

   int point_square = (point >> 16) & 0xffff;

   return roadmap_square_from_index (point_square);
}
*/

