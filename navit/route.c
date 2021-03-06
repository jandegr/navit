/* some changes in routing, read :
 * #630 ???
 * #876 bad or no route found
 * #1063 sharp turns
 * #1085
 * #1162, #1047, #891 traffic lights
 * #1189 don't leave motorway over emergency access road
 * #1205 cost of turns in routing
 * #1279 some routing problems
 * #1323
 *
 * HOV restriction
 */

/* jandegr 2015 */


/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/** @file
 * @brief Contains code related to finding a route from a position to a destination
 *
 * Routing uses segments, points and items. Items are items from the map: Streets, highways, etc.
 * Segments represent such items, or parts of it. Generally, a segment is a driveable path. An item
 * can be represented by more than one segment - in that case it is "segmented". Each segment has an
 * "offset" associated, that indicates at which position in a segmented item this segment is - a 
 * segment representing a not-segmented item always has the offset 1.
 * A point is located at the end of segments, often connecting several segments.
 * 
 * The code in this file will make navit find a route between a position and a destination.
 * It accomplishes this by first building a "route graph". This graph contains segments and
 * points.
 *
 * After building this graph in route_graph_build(), the function route_graph_flood() assigns every 
 * point and segment a "value" which represents the "costs" of traveling from this point to the
 * destination. This is done by Dijkstra's algorithm.
 *
 * When the graph is built a "route path" is created, which is a path in this graph from a given
 * position to the destination determined at time of building the graph.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if 0
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#endif
#include "navit_nls.h"
#include "glib_slice.h"
#include "config.h"
#include "point.h"
#include "graphics.h"
#include "profile.h"
#include "coord.h"
#include "projection.h"
#include "item.h"
#include "xmlconfig.h"
#include "map.h"
#include "mapset.h"
#include "route.h"
#include "track.h"
#include "transform.h"
#include "plugin.h"
#include "fib.h"
#include "event.h"
#include "callback.h"
#include "vehicle.h"
#include "vehicleprofile.h"
#include "roadprofile.h"
#include "debug.h"

struct map_priv {
    struct route *route;
};


int heuristic_speed = INT_MAX; // in km/h for A*

enum route_path_flags {
    route_path_flag_none=0,
    route_path_flag_cancel=1,
    route_path_flag_async=2,
    route_path_flag_no_rebuild=4,
};


/**
 * @brief A point in the route graph
 *
 * This represents a point in the route graph. A point usually connects two or more segments,
 * but there are also points which don't do that (e.g. at the end of a dead-end).
 */
struct route_graph_point {
    struct route_graph_point *hash_next; /**< Pointer to a chained hashlist of all route_graph_points with this hash */
    struct route_graph_segment *start;   /**< Pointer to a list of segments of which this point is the start. The links 
                                          *  of this linked-list are in route_graph_segment->start_next.*/
    struct route_graph_segment *end;     /**< Pointer to a list of segments of which this pointer is the end. The links
                                          *  of this linked-list are in route_graph_segment->end_next. */
    struct coord c;                      /**< Coordinates of this point */
    int flags;                      /**< Flags for this point (eg traffic distortion) */
};

#define RP_TRAFFIC_DISTORTION 1
#define RP_TURN_RESTRICTION 2
//#define RP_TURN_RESTRICTION_RESOLVED 4


/**
 * @brief A segment in the route graph or path
 *
 * This is a segment in the route graph or path. A segment represents a driveable way.
 */

struct route_segment_data {
    struct item item;                           /**< The item (e.g. street) that this segment represents. */
    int flags;
    int len;
    int angle_start;
    int angle_end;

    /**< Length of this segment */
    /*NOTE: After a segment, various fields may follow, depending on what flags are set. Order of fields:
                1.) maxspeed            Maximum allowed speed on this segment. Present if AF_SPEED_LIMIT is set.
                2.) offset              If the item is segmented (i.e. represented by more than one segment), this
                                        indicates the position of this segment in the item. Present if AF_SEGMENTED is set.
     */
};


struct size_weight_limit {
    int width;
    int length;
    int height;
    int weight;
    int axle_weight;
};

#define RSD_OFFSET(x) *((int *)route_segment_data_field_pos((x), attr_offset))
#define RSD_MAXSPEED(x) *((int *)route_segment_data_field_pos((x), attr_maxspeed))
#define RSD_SIZE_WEIGHT(x) *((struct size_weight_limit *)route_segment_data_field_pos((x), attr_vehicle_width))
#define RSD_DANGEROUS_GOODS(x) *((int *)route_segment_data_field_pos((x), attr_vehicle_dangerous_goods))


struct route_graph_segment_data {
    struct item *item;
    int offset;
    int flags;
    int len;
    int maxspeed;
    struct size_weight_limit size_weight;
    int dangerous_goods;
    int angle_start;
    int angle_end;
};

/**
 * @brief A segment in the route graph
 *
 * This is a segment in the route graph. A segment represents a driveable way.
 */
struct route_graph_segment {
    struct route_graph_segment *next;           /**< Linked-list pointer to a list of all route_graph_segments */
    struct route_graph_segment *start_next;     /**< Pointer to the next element in the list of segments that start at the 
                                                 *  same point. Start of this list is in route_graph_point->start. */
    struct route_graph_segment *end_next;       /**< Pointer to the next element in the list of segments that end at the
                                                 *  same point. Start of this list is in route_graph_point->end. */
    struct route_graph_point *start;            /**< Pointer to the point this segment starts at. */
    struct route_graph_point *end;              /**< Pointer to the point this segment ends at. */

    int seg_start_out_cost;
    int seg_end_out_cost;

    struct route_graph_segment *start_from_seg;
    struct route_graph_segment *end_from_seg;

    struct fibheap_el *el_start;
    struct fibheap_el *el_end;

    struct route_segment_data data;             /**< The segment data */


};

/**
 * @brief A traffic distortion
 *
 * This is distortion in the traffic where you can't drive as fast as usual or have to wait for some time
 */
struct route_traffic_distortion {
    int maxspeed;                   /**< Maximum speed possible in km/h */
    int delay;                  /**< Delay in tenths of seconds */
};

/**
 * @brief A segment in the route path
 *
 * This is a segment in the route path.
 */
struct route_path_segment {
    struct route_path_segment *next;    /**< Pointer to the next segment in the path */
    struct route_segment_data *data;    /**< The segment data */
    int direction;                      /**< Order in which the coordinates are ordered. >0 means "First
                                         *  coordinate of the segment is the first coordinate of the item", <=0 
                                         *  means reverse. */
    unsigned ncoords;                   /**< How many coordinates does this segment have? */
    struct coord c[0];                  /**< Pointer to the ncoords coordinates of this segment */
    /* WARNING: There will be coordinates following here, so do not create new fields after c! */
};

/**
 * @brief Usually represents a destination or position
 *
 * This struct usually represents a destination or position
 */
struct route_info {
    struct coord c;          /**< The actual destination / position */
    struct coord lp;         /**< The nearest point on a street to c */
    int pos;                 /**< The position of lp within the coords of the street */
    int lenpos;              /**< Distance between lp and the end of the street */
    int lenneg;              /**< Distance between lp and the start of the street */
    int lenextra;            /**< Distance between lp and c */
    int percent;             /**< ratio of lenneg to lenght of whole street in percent */
    struct street_data *street; /**< The street lp is on */
    int street_direction;   /**< Direction of vehicle on street -1 = Negative direction, 1 = Positive direction, 0 = Unknown */
    int dir;        /**< Direction to take when following the route -1 = Negative direction, 1 = Positive direction */
};

/**
 * @brief A complete route path
 *
 * This structure describes a whole routing path
 */
struct route_path {
    int in_use;                     /**< The path is in use and can not be updated */
    int update_required;                    /**< The path needs to be updated after it is no longer in use */
    int updated;                        /**< The path has only been updated */
    int path_time;                      /**< Time to pass the path */
    int path_len;                       /**< Length of the path */
    struct route_path_segment *path;            /**< The first segment in the path, i.e. the segment one should 
                                                 *  drive in next */
    struct route_path_segment *path_last;       /**< The last segment in the path */
    struct item_hash *path_hash;                /**< A hashtable of all the items represented by this route's segements */
    struct route_path *next;                /**< Next route path in case of intermediate destinations */    
};

/**
 * @brief A complete route
 * 
 * This struct holds all information about a route.
 */
struct route {
    NAVIT_OBJECT
    struct mapset *ms;          /**< The mapset this route is built upon */
    unsigned flags;
    struct route_info *pos;     /**< Current position within this route */
    GList *destinations;        /**< Destinations of the route */
    int reached_destinations_count; /**< Used as base to calculate waypoint numbers */
    struct route_info *current_dst; /**< Current destination */

    struct route_graph *graph;  /**< Pointer to the route graph */
    struct route_path *path2;   /**< Pointer to the route path */
    struct map *map;
    struct map *graph_map;
    struct callback * route_graph_done_cb ; /**< Callback when route graph is done */
//  struct callback * route_graph_flood_done_cb ; /**< Callback when route graph flooding is done */
    struct callback_list *cbl2; /**< Callback list to call when route changes */
    int destination_distance;   /**< Distance to the destination at which the destination is considered "reached" */
    struct vehicleprofile *vehicleprofile; /**< Routing preferences */
    int route_status;       /**< Route Status */
    int link_path;          /**< Link paths over multiple waypoints together */
    struct pcoord pc;
    struct vehicle *v;
};

/**
 * @brief A complete route graph
 *
 * This structure describes a whole routing graph
 */
struct route_graph {
    int busy;                   /**< The graph is being built */
    struct map_selection *sel;          /**< The rectangle selection for the graph */
    struct mapset_handle *h;            /**< Handle to the mapset */    
    struct map *m;                  /**< Pointer to the currently active map */ 
    struct map_rect *mr;                /**< Pointer to the currently active map rectangle */
    struct vehicleprofile *vehicleprofile;      /**< The vehicle profile */
//  struct callback *idle_cb;           /**< Idle callback to process the graph */
    struct callback *done_cb;           /**< Callback when graph is done */
//  struct event_idle *idle_ev;         /**< The pointer to the idle event */
    struct route_graph_segment *route_segments; /**< Pointer to the first route_graph_segment in the linked list of all segments */
    struct route_graph_segment *avoid_seg;
#define HASH_SIZE 8192
    struct route_graph_point *hash[HASH_SIZE];  /**< A hashtable containing all route_graph_points in this graph */
};

#define HASHCOORD(c) ((((c)->x +(c)->y) * 2654435761UL) & (HASH_SIZE-1))

/**
 * @brief Iterator to iterate through all route graph segments in a route graph point
 *
 * This structure can be used to iterate through all route graph segments connected to a
 * route graph point. Use this with the rp_iterator_* functions.
 */
struct route_graph_point_iterator {
    struct route_graph_point *p;        /**< The route graph point whose segments should be iterated */
    int end;                            /**< Indicates if we have finished iterating through the "start" segments */
    struct route_graph_segment *next;   /**< The next segment to be returned */
};

struct attr_iter {
    union {
        GList *list;
    } u;
};

static struct route_info * route_find_nearest_street(struct vehicleprofile *vehicleprofile, struct mapset *ms, struct pcoord *c);
static struct route_graph_point *route_graph_get_point(struct route_graph *this, struct coord *c);
static void route_graph_update(struct route *this, struct callback *cb, int async);
static void route_graph_build_done(struct route_graph *rg, int cancel);
static struct route_path *route_path_new(struct route_graph *this, struct route_path *oldpath, struct route_info *pos, struct route_info *dst, struct vehicleprofile *profile);
static void route_graph_add_item(struct route_graph *this, struct item *item, struct vehicleprofile *profile);
static void route_graph_destroy(struct route_graph *this);
static void route_path_update(struct route *this, int cancel, int async);
static int route_time_seg(struct vehicleprofile *profile, struct route_segment_data *over, struct route_traffic_distortion *dist);
static void route_graph_flood_frugal(struct route_graph *this, struct route_info *dst, struct route_info *pos, struct vehicleprofile *profile, struct callback *cb);
static void route_graph_reset(struct route_graph *this);
static struct route_graph_segment * route_graph_get_segment(struct route_graph *graph, struct street_data *sd, struct route_graph_segment *last);
static int is_turn_allowed(struct route_graph_point *p, struct route_graph_segment *from, struct route_graph_segment *to);



inline double now_ms()
{
 //   struct timeval tv;
 //   gettimeofday(&tv, NULL);
 //   return tv.tv_sec*1000. + tv.tv_usec/1000.;
 
 // arm64 implicit declaration
 
 return 0;
 
}

/**
 * @brief Returns the projection used for this route
 *
 * @param route The route to return the projection for
 * @return The projection used for this route
 */
static enum projection route_projection(struct route *route)
{
    struct street_data *street;
    struct route_info *dst=route_get_dst(route);
    if (!route->pos && !dst)
        return projection_none;
    street = route->pos ? route->pos->street : dst->street;
    if (!street || !street->item.map)
        return projection_none;
    return map_projection(street->item.map);
}

/**
 * @brief Creates a new graph point iterator 
 *
 * This function creates a new route graph point iterator, that can be used to
 * iterate through all segments connected to the point.
 *
 * @param p The route graph point to create the iterator from
 * @return A new iterator.
 */
static struct route_graph_point_iterator
rp_iterator_new(struct route_graph_point *p)
{
    struct route_graph_point_iterator it;

    it.p = p;
    if (p->start) {
        it.next = p->start;
        it.end = 0;
    } else {
        it.next = p->end;
        it.end = 1;
    }

    return it;
}

/**
 * @brief Gets the next segment connected to a route graph point from an iterator
 *
 * @param it The route graph point iterator to get the segment from
 * @return The next segment or NULL if there are no more segments
 */
static struct route_graph_segment
*rp_iterator_next(struct route_graph_point_iterator *it) 
{
    struct route_graph_segment *ret;

    ret = it->next;
    if (!ret) {
        return NULL;
    }

    if (!it->end) {
        if (ret->start_next) {
            it->next = ret->start_next;
        } else {
            it->next = it->p->end;
            it->end = 1;
        }
    } else {
        it->next = ret->end_next;
    }

    return ret;
}

static void
route_path_get_distances(struct route_path *path, struct coord *c, int count, int *distances)
{
    int i;
    for (i = 0 ; i < count ; i++)
        distances[i]=INT_MAX;
    while (path)
    {
        struct route_path_segment *seg=path->path;
        while (seg)
        {
            for (i = 0 ; i < count ; i++)
            {
                int dist=transform_distance_polyline_sq(seg->c, seg->ncoords, &c[i], NULL, NULL);
                if (dist < distances[i]) 
                    distances[i]=dist;
            }
            seg=seg->next;
        }
        path=path->next;
    }
    for (i = 0 ; i < count ; i++)
    {
        if (distances[i] != INT_MAX)
            distances[i]=sqrt(distances[i]);
    }
}

void
route_get_distances(struct route *this, struct coord *c, int count, int *distances)
{
    return route_path_get_distances(this->path2, c, count, distances);
}

/**
 * @brief Destroys a route_path
 *
 * @param this The route_path to be destroyed
 */
static void
route_path_destroy(struct route_path *this, int recurse)
{
    struct route_path_segment *c,*n;
    struct route_path *next;
    while (this)
    {
        next=this->next;
        if (this->path_hash)
        {
            item_hash_destroy(this->path_hash);
            this->path_hash=NULL;
        }
        c=this->path;
        while (c)
        {
            n=c->next;
            g_free(c);
            c=n;
        }
        this->in_use--;
        if (!this->in_use)
            g_free(this);
        if (!recurse)
            break;
        this=next;
    }
}

/**
 * @brief Creates a completely new route structure
 *
 * @param attrs Not used
 * @return The newly created route
 */
struct route *
route_new(struct attr *parent, struct attr **attrs)
{
    struct route *this=g_new0(struct route, 1);
    struct attr dest_attr;

    this->func=&route_func;
        navit_object_ref((struct navit_object *)this);

    if (attr_generic_get_attr(attrs, NULL, attr_destination_distance, &dest_attr, NULL))
    {
        this->destination_distance = dest_attr.u.num;
    }
    else
    {
        this->destination_distance = 50; // Default value
    }
    this->cbl2=callback_list_new();

    return this;
}


/**
 * @brief Duplicates a route object
 *
 * @return The duplicated route
 */

struct route *
route_dup(struct route *orig)
{
//  struct route *this=g_new0(struct route, 1);
//  this->func=&route_func;
//        navit_object_ref((struct navit_object *)this);
//  this->cbl2=callback_list_new();
//  this->destination_distance=orig->destination_distance;
//  this->ms=orig->ms;
//  this->flags=orig->flags;
//  this->vehicleprofile=orig->vehicleprofile;

//  return this;
    return NULL;
}


/**
 * @brief Sets the mapset of the route passwd
 *
 * @param this The route to set the mapset for
 * @param ms The mapset to set for this route
 */
void
route_set_mapset(struct route *this, struct mapset *ms)
{
    this->ms=ms;
}

/**
 * @brief Sets the vehicle profile of a route
 *
 * @param this The route to set the profile for
 * @param prof The vehicle profile
 */

void
route_set_profile(struct route *this, struct vehicleprofile *prof)
{
    if (this->vehicleprofile != prof)
    {
        int dest_count = g_list_length(this->destinations);
        struct pcoord *pc;
        this->vehicleprofile = prof;
        pc = g_alloca(dest_count*sizeof(struct pcoord));
        route_get_destinations(this, pc, dest_count);
        route_set_destinations(this, pc, dest_count, 1);
    }
}

/**
 * @brief Returns the mapset of the route passed
 *
 * @param this The route to get the mapset of
 * @return The mapset of the route passed
 */
struct mapset *
route_get_mapset(struct route *this)
{
    return this->ms;
}

/**
 * @brief Returns the current position within the route passed
 *
 * @param this The route to get the position for
 * @return The position within the route passed
 */
struct route_info *
route_get_pos(struct route *this)
{
    return this->pos;
}

/**
 * @brief Returns the destination of the route passed
 *
 * @param this The route to get the destination for
 * @return The destination of the route passed
 */
struct route_info *
route_get_dst(struct route *this)
{
    struct route_info *dst=NULL;

    if (this->destinations)
        dst=g_list_last(this->destinations)->data;
    return dst;
}

/**
 * @brief Checks if the path is calculated for the route passed
 *
 * @param this The route to check
 * @return True if the path is calculated, false if not
 */
int
route_get_path_set(struct route *this)
{
    return this->path2 != NULL;
}

/**
 * @brief Checks if the route passed contains a certain item within the route path
 *
 * This function checks if a certain items exists in the path that navit will guide
 * the user to his destination. It does *not* check if this item exists in the route 
 * graph!
 *
 * @param this The route to check for this item
 * @param item The item to search for
 * @return True if the item was found, false if the item was not found or the route was not calculated
 */
int
route_contains(struct route *this, struct item *item)
{
    if (! this->path2 || !this->path2->path_hash)
        return 0;
    if (item_hash_lookup(this->path2->path_hash, item))
        return 1;
    if (! this->pos || !this->pos->street)
        return 0;
    return item_is_equal(this->pos->street->item, *item);

}

static struct route_info *
route_next_destination(struct route *this)
{
    if (!this->destinations)
        return NULL;
    return this->destinations->data;
}

/**
 * @brief Checks if a route has reached its destination
 *
 * @param this The route to be checked
 * @return 0 if destination not reached
 * @return 1 if a destination is reached, but one or more destinations following
 * @return 2 is destination is reached and it was the last or only one
 */
int
route_destination_reached(struct route *this)
{
    struct street_data *sd = NULL;
    enum projection pro;
    struct route_info *dst=route_next_destination(this);

    if (!this->pos)
        return 0;
    if (!dst)
        return 0;
    
    sd = this->pos->street;

    if (!this->path2)
    {
        return 0;
    }

    if (!item_is_equal(this->pos->street->item, dst->street->item))
    {
        return 0;
    }

    if ((sd->flags & AF_ONEWAY) && (this->pos->lenneg >= dst->lenneg))
    { // We would have to drive against the one-way road
        return 0;
    }
    if ((sd->flags & AF_ONEWAYREV) && (this->pos->lenpos >= dst->lenpos))
    {
        return 0;
    }
    pro=route_projection(this);
    if (pro == projection_none)
        return 0;
     
    if (transform_distance(pro, &this->pos->c, &dst->lp) > this->destination_distance)
    {
        return 0;
    }

    if (g_list_next(this->destinations))    
        return 1;
    else
        return 2;
}

static struct route_info *
route_previous_destination(struct route *this)
{
    GList *l=g_list_find(this->destinations, this->current_dst);
    if (!l)
        return this->pos;
    l=g_list_previous(l);
    if (!l)
        return this->pos;
    return l->data;
}

static void
route_path_update_done(struct route *this, int new_graph)
{
    struct route_path *oldpath=this->path2;
    struct attr route_status;
    struct route_info *prev_dst;
    route_status.type=attr_route_status;

    dbg(lvl_debug,"route Path update done START, new_graph = %i\n",new_graph);


    if (this->path2 && (this->path2->in_use>1))
    {
        this->path2->update_required=1+new_graph;
        return;
    }
    route_status.u.num=route_status_building_path;
    route_set_attr(this, &route_status);
    prev_dst=route_previous_destination(this);
    if (this->link_path)
    {
        dbg(lvl_debug,"route Path update done ZONDER oldpath\n");
        this->path2=route_path_new(this->graph, NULL, prev_dst, this->current_dst, this->vehicleprofile);
        if (this->path2)
            this->path2->next=oldpath;
        else
            route_path_destroy(oldpath,0);
    }
    else
    {
        dbg(lvl_debug,"route Path update done MET oldpath\n");
        this->path2=route_path_new(this->graph, oldpath, prev_dst, this->current_dst, this->vehicleprofile);
        if (oldpath && this->path2)
        {
            this->path2->next=oldpath->next;
            route_path_destroy(oldpath,0);
        }
    }
    if (this->path2)
    {
        struct route_path_segment *seg=this->path2->path;
        int path_time=0,path_len=0;
        while (seg)
        {
            /* FIXME */
            int seg_time=route_time_seg(this->vehicleprofile, seg->data, NULL);
            if (seg_time == INT_MAX)
            {
                dbg(lvl_debug,"segment time error\n");
                if (this->pos->c.x == seg->c[0].x && this->pos->c.y == seg->c[0].y )
                {   //zit niet in profile, zelfde mogelijk bij destinations
                    dbg(lvl_error,"pos lennextra segment\n");
                }
            }
            else
            {
                path_time+=seg_time;
            }
            path_len+=seg->data->len;
            seg=seg->next;
        }
        this->path2->path_time=path_time;
        this->path2->path_len=path_len;
        if (prev_dst != this->pos)
        {
            this->link_path=1;
            this->current_dst=prev_dst;
            route_graph_reset(this->graph);
//          route_graph_flood_frugal(this->graph, this->current_dst, this->pos, this->vehicleprofile, this->route_graph_flood_done_cb);
            route_graph_flood_frugal(this->graph, this->current_dst, this->pos, this->vehicleprofile, NULL);
            // moet er hier nu ook nog een aanroep tussen ?
            // route_path_update_done(this,1); // 1 = int new_graph
            return;
        }
        if (!new_graph && this->path2->updated)
            route_status.u.num=route_status_path_done_incremental; //33
        else
            route_status.u.num=route_status_path_done_new; //17
    }
    else
    {
        route_status.u.num=route_status_not_found;
    }
    this->link_path=0;
    route_set_attr(this, &route_status);
}

/**
 * @brief Updates the route graph and the route path if something changed with the route
 *
 * This will update the route graph and the route path of the route if some of the
 * route's settings (destination, position) have changed. 
 * 
 * @attention For this to work the route graph has to be destroyed if the route's 
 * @attention destination is changed somewhere!
 *
 * @param this The route to update
 */
static void
route_path_update_flags(struct route *this, enum route_path_flags flags)
{
    dbg(lvl_debug,"enter %d\n", flags);
    if (! this->pos || ! this->destinations)
    {
        dbg(lvl_debug,"destroy\n");
        route_path_destroy(this->path2,1);
        this->path2 = NULL;
        return;
    }
    if (flags & route_path_flag_cancel)
    {
        route_graph_destroy(this->graph);
        this->graph=NULL;
    }
    /* the graph is destroyed when setting the destination */
    if (this->graph)
    {
        if (this->graph->busy)
        {
            dbg(lvl_debug,"busy building graph\n");
            return;
        }
        // we can try to update
        route_path_update_done(this, 0);
    }
    else
    {
        route_path_destroy(this->path2,1);
        this->path2 = NULL;
    }
    if (!this->graph || (!this->path2 && !(flags & route_path_flag_no_rebuild)))
    {
        dbg(lvl_debug,"rebuild graph %p %p\n",this->graph,this->path2);
//      if (! this->route_graph_flood_done_cb)
//          this->route_graph_flood_done_cb=callback_new_2(callback_cast(route_path_update_done), this, (long)1);
        dbg(lvl_debug,"route_graph_update\n");
//      route_graph_update(this, this->route_graph_flood_done_cb, !!(flags & route_path_flag_async));
        route_graph_update(this, NULL, 0); // 0 = async
        route_path_update_done(this,1); // 1 = int new_graph
    }
}

static void
route_path_update(struct route *this, int cancel, int async)
{
    async = 0; //testje
    enum route_path_flags flags=(cancel ? route_path_flag_cancel:0)|(async ? route_path_flag_async:0);
    route_path_update_flags(this, flags);
}


/** 
 * @brief This will calculate all the distances stored in a route_info
 *
 * @param ri The route_info to calculate the distances for
 * @param pro The projection used for this route
 */
static void
route_info_distances(struct route_info *ri, enum projection pro)
{
    int npos=ri->pos+1;
    struct street_data *sd=ri->street;
    /* 0 1 2 X 3 4 5 6 pos=2 npos=3 count=7 0,1,2 3,4,5,6*/
    ri->lenextra=transform_distance(pro, &ri->lp, &ri->c);
    ri->lenneg=transform_polyline_length(pro, sd->c, npos)+transform_distance(pro, &sd->c[ri->pos], &ri->lp);
    ri->lenpos=transform_polyline_length(pro, sd->c+npos, sd->count-npos)+transform_distance(pro, &sd->c[npos], &ri->lp);
    if (ri->lenneg || ri->lenpos)
        ri->percent=(ri->lenneg*100)/(ri->lenneg+ri->lenpos);
    else
        ri->percent=50;
}

/**
 * @brief This sets the current position of the route passed
 *
 * This will set the current position of the route passed to the street that is nearest to the
 * passed coordinates. It also automatically updates the route.
 *
 * @param this The route to set the position of
 * @param pos Coordinates to set as position
 * @param flags Flags to use for building the graph
 */

static int
route_set_position_flags(struct route *this, struct pcoord *pos, enum route_path_flags flags)
{
    if (this->pos)
        route_info_free(this->pos);
    this->pos=NULL;
    this->pos=route_find_nearest_street(this->vehicleprofile, this->ms, pos);

    // If there is no nearest street, bail out.
    if (!this->pos) return 0;

    this->pos->street_direction=0;
    dbg(lvl_debug,"this->pos=%p\n", this->pos);
    route_info_distances(this->pos, pos->pro);
//  route_path_update_flags(this, flags);
    route_path_update_flags(this, 0);
    return 1;
}

/**
 * @brief This sets the current position of the route passed
 *
 * This will set the current position of the route passed to the street that is nearest to the
 * passed coordinates. It also automatically updates the route.
 *
 * @param this The route to set the position of
 * @param pos Coordinates to set as position
 */
void
route_set_position(struct route *this, struct pcoord *pos)
{
//  route_set_position_flags(this, pos, route_path_flag_async);
    route_set_position_flags(this, pos, 0);
}

/**
 * @brief Sets a route's current position based on coordinates from tracking
 *
 * @param this The route to set the current position of
 * @param tracking The tracking to get the coordinates from
 */
void
route_set_position_from_tracking(struct route *this, struct tracking *tracking, enum projection pro)
{
    struct coord *c;
    struct route_info *ret;
    struct street_data *sd;

    c=tracking_get_pos(tracking);
    ret=g_new0(struct route_info, 1);
    if (!ret)
    {
        printf("%s:Out of memory\n", __FUNCTION__);
        return;
    }
    if (this->pos)
        route_info_free(this->pos);
    this->pos=NULL;
    ret->c=*c;
    ret->lp=*c;
    ret->pos=tracking_get_segment_pos(tracking);
    ret->street_direction=tracking_get_street_direction(tracking);
    sd=tracking_get_street_data(tracking);
    if (sd)
    {
        ret->street=street_data_dup(sd);
        route_info_distances(ret, pro);
    }
    dbg(lvl_debug,"position 0x%x,0x%x item 0x%x,0x%x direction %d pos %d lenpos %d lenneg %d\n",c->x,c->y,sd?sd->item.id_hi:0,sd?sd->item.id_lo:0,ret->street_direction,ret->pos,ret->lenpos,ret->lenneg);
    dbg(lvl_debug,"c->x=0x%x, c->y=0x%x pos=%d item=(0x%x,0x%x)\n", c->x, c->y, ret->pos, ret->street?ret->street->item.id_hi:0, ret->street?ret->street->item.id_lo:0);
    dbg(lvl_debug,"street 0=(0x%x,0x%x) %d=(0x%x,0x%x)\n", ret->street?ret->street->c[0].x:0, ret->street?ret->street->c[0].y:0, ret->street?ret->street->count-1:0, ret->street?ret->street->c[ret->street->count-1].x:0, ret->street?ret->street->c[ret->street->count-1].y:0);
    this->pos=ret;
    if (this->destinations) 
        route_path_update(this, 0, 1);
}

/* Used for debuging of route_rect, what routing sees */
struct map_selection *route_selection;

/**
 * @brief Returns a single map selection
 */
struct map_selection *
route_rect(int order, struct coord *c1, struct coord *c2, int rel, int abs)
{
    int dx,dy,sx=1,sy=1,d,m;
    struct map_selection *sel=g_new(struct map_selection, 1);
    if (!sel)
    {
        printf("%s:Out of memory\n", __FUNCTION__);
        return sel;
    }
    sel->order=order;
    sel->range.min=route_item_first;
    sel->range.max=route_item_last;
    dbg(lvl_debug,"%p %p\n", c1, c2);
    dx=c1->x-c2->x;
    dy=c1->y-c2->y;
    if (dx < 0)
    {
        sx=-1;
        sel->u.c_rect.lu.x=c1->x;
        sel->u.c_rect.rl.x=c2->x;
    }
    else
    {
        sel->u.c_rect.lu.x=c2->x;
        sel->u.c_rect.rl.x=c1->x;
    }
    if (dy < 0)
    {
        sy=-1;
        sel->u.c_rect.lu.y=c2->y;
        sel->u.c_rect.rl.y=c1->y;
    }
    else
    {
        sel->u.c_rect.lu.y=c1->y;
        sel->u.c_rect.rl.y=c2->y;
    }
    if (dx*sx > dy*sy) 
        d=dx*sx;
    else
        d=dy*sy;
    m=d*rel/100+abs;
    sel->u.c_rect.lu.x-=m;
    sel->u.c_rect.rl.x+=m;
    sel->u.c_rect.lu.y+=m;
    sel->u.c_rect.rl.y-=m;
    sel->next=NULL;
    return sel;
}

/**
 * @brief Appends a map selection to the selection list. Selection list may be NULL.
 */
static struct map_selection *
route_rect_add(struct map_selection *sel, int order, struct coord *c1, struct coord *c2, int rel, int abs)
{
    struct map_selection *ret;
    ret=route_rect(order, c1, c2, rel, abs);
    ret->next=sel;
    return ret;
}

/**
 * @brief Returns a list of map selections useable to create a route graph
 *
 * Returns a list of  map selections useable to get a map rect from which items can be
 * retrieved to build a route graph. 
 *
 * @param c Array containing route points, including start, intermediate and destination ones.
 * @param count number of route points 
 * @param proifle vehicleprofile 
 */
static struct map_selection *
route_calc_selection(struct coord *c, int count, struct vehicleprofile *profile)
{
    struct map_selection *ret=NULL;
    int i;
    struct coord_rect r;
    char *depth, *str, *tok;

    if (!count)
        return NULL;
    r.lu=c[0];
    r.rl=c[0];
    for (i = 1 ; i < count ; i++)
        coord_rect_extend(&r, &c[i]);

    depth=profile->route_depth;
    if (!depth)
        depth="4:25%,8:40000,18:10000";
    depth=str=g_strdup(depth);
    
    while((tok=strtok(str,","))!=NULL)
    {
        int order=0, dist=0;
        sscanf(tok,"%d:%d",&order,&dist);
        if(strchr(tok,'w'))
            ret=route_rect_add(ret, order, &r.lu, &r.rl, 0,dist);
        else
        {
            if(strchr(tok,'%'))
            {
                ret=route_rect_add(ret, order, &r.lu, &r.rl, dist, 0);
            }
            else
            {
                for (i = 0 ; i < count ; i++)
                {
                    ret=route_rect_add(ret, order, &c[i], &c[i], 0, dist);
                }
            }
        }
        str=NULL;
    }
    g_free(depth);
    
    return ret;
}

/**
 * @brief Destroys a list of map selections
 *
 * @param sel Start of the list to be destroyed
 */
static void
route_free_selection(struct map_selection *sel)
{
    struct map_selection *next;
    while (sel)
    {
        next=sel->next;
        g_free(sel);
        sel=next;
    }
}


static void
route_clear_destinations(struct route *this_)
{
    g_list_foreach(this_->destinations, (GFunc)route_info_free, NULL);
    g_list_free(this_->destinations);
    this_->destinations=NULL;
}

/**
 * @brief Sets the destination of a route
 *
 * This sets the destination of a route to the street nearest to the coordinates passed
 * and updates the route.
 *
 * @param this The route to set the destination for
 * @param dst Coordinates to set as destination
 * @param count: Number of destinations (last one is final)
 * @param async: If set, do routing asynchronously
 */

void
route_set_destinations(struct route *this, struct pcoord *dst, int count, int async)
{
    struct attr route_status;
    struct route_info *dsti;
    int i;
    route_status.type=attr_route_status;

    route_clear_destinations(this);
    if (dst && count)
    {
        for (i = 0 ; i < count ; i++)
        {
            dsti=route_find_nearest_street(this->vehicleprofile, this->ms, &dst[i]);
            if(dsti)
            {
                route_info_distances(dsti, dst->pro);
                this->destinations=g_list_append(this->destinations, dsti);
            }
        }
        route_status.u.num=route_status_destination_set;
    }
    else
    {
        this->reached_destinations_count=0;
        route_status.u.num=route_status_no_destination;
    }
    callback_list_call_attr_1(this->cbl2, attr_destination, this);
    route_set_attr(this, &route_status);

    /* The graph has to be destroyed and set to NULL, otherwise route_path_update() doesn't work */
    route_graph_destroy(this->graph);
    this->graph=NULL;
    this->current_dst=route_get_dst(this);
//  route_path_update(this, 1, async);
    route_path_update(this, 1, 0);
}

int
route_get_destinations(struct route *this, struct pcoord *pc, int count)
{
    int ret=0;
    GList *l=this->destinations;
    while (l && ret < count)
    {
        struct route_info *dst=l->data;
        pc->x=dst->c.x;
        pc->y=dst->c.y;
        pc->pro=projection_mg; /* FIXME */
        pc++;
        ret++;
        l=g_list_next(l);
    }
    return ret;
}

/**
 * @brief Get the destinations count for the route
 *
 * @param this The route instance
 * @return destination count for the route
 */
int
route_get_destination_count(struct route *this)
{
    return g_list_length(this->destinations);
}

/**
 * @brief Returns a description for a waypoint as (type or street_name_systematic) + (label or WayID[osm_wayid])
 *
 * @param this The route instance
 * @param n The nth waypoint
 * @return The description
 */
char*
route_get_destination_description(struct route *this, int n)
{
    struct route_info *dst;
    struct map_rect *mr=NULL;
    struct item *item;
    struct attr attr;
    char *type=NULL;
    char *label=NULL;
    char *desc=NULL;

    if(!this->destinations)
        return NULL;

    dst=g_list_nth_data(this->destinations,n);
    mr=map_rect_new(dst->street->item.map, NULL);
    item = map_rect_get_item_byid(mr, dst->street->item.id_hi, dst->street->item.id_lo);

    type=g_strdup(item_to_name(dst->street->item.type));

    while(item_attr_get(item, attr_any, &attr))
    {
        if (attr.type==attr_street_name_systematic )
        {
            g_free(type);
            type=attr_to_text(&attr, item->map, 1);
        }
        else if (attr.type==attr_label)
        {
            g_free(label);
            label=attr_to_text(&attr, item->map, 1);
        }
        else if (attr.type==attr_osm_wayid && !label)
        {
            char *tmp=attr_to_text(&attr, item->map, 1);
            label=g_strdup_printf("WayID %s", tmp);
            g_free(tmp);
        }
    }

    if(!label && !type)
    {
        desc=g_strdup(_("unknown street"));
    }
    else if (!label || strcmp(type, label)==0)
    {
        desc=g_strdup(type);
    }
    else
    {
        desc=g_strdup_printf("%s %s", type, label);
    }

    g_free(label);
    g_free(type);

    if (mr)
        map_rect_destroy(mr);

    return desc;
}

/**
 * @brief Start a route given set of coordinates
 *
 * @param this The route instance
 * @param c The coordinate to start routing to
 * @param async 1 for async
 * @return nothing
 */
void
route_set_destination(struct route *this, struct pcoord *dst, int async)
{
//  route_set_destinations(this, dst, dst?1:0, async);
    route_set_destinations(this, dst, dst?1:0, 0);
}

/**
 * @brief Append a waypoint to the route.
 *
 * This appends a waypoint to the current route, targetting the street
 * nearest to the coordinates passed, and updates the route.
 * 
 * @param this The route to set the destination for
 * @param dst Coordinates of the new waypoint
 * @param async: If set, do routing asynchronously
 */
void
route_append_destination(struct route *this, struct pcoord *dst, int async)
{
    if (dst)
    {
        struct route_info *dsti;
        dsti=route_find_nearest_street(this->vehicleprofile, this->ms, &dst[0]);
        if(dsti)
        {
            route_info_distances(dsti, dst->pro);
            this->destinations=g_list_append(this->destinations, dsti);
        }
        /* The graph has to be destroyed and set to NULL, otherwise route_path_update() doesn't work */
        route_graph_destroy(this->graph);
        this->graph=NULL;
        this->current_dst=route_get_dst(this);
        route_path_update(this, 1, async);
    }
    else
    {
        route_set_destinations(this, NULL, 0, async);
    }
}

/**
 * @brief Remove the nth waypoint of the route
 *
 * @param this The route instance
 * @param n The waypoint to remove
 * @return nothing
 */
void
route_remove_nth_waypoint(struct route *this, int n)
{
    struct route_info *ri=g_list_nth_data(this->destinations, n);
    this->destinations=g_list_remove(this->destinations,ri);
    route_info_free(ri);
    /* The graph has to be destroyed and set to NULL, otherwise route_path_update() doesn't work */
    route_graph_destroy(this->graph);
    this->graph=NULL;
    this->current_dst=route_get_dst(this);
    route_path_update(this, 1, 1);
}

void
route_remove_waypoint(struct route *this)
{
    if (this->path2) {
        struct route_path *path = this->path2;
        struct route_info *ri = this->destinations->data;
        this->destinations = g_list_remove(this->destinations, ri);
        route_info_free(ri);
        this->path2 = this->path2->next;
        route_path_destroy(path, 0);
        if (!this->destinations)
        {
            this->route_status=route_status_no_destination;
            this->reached_destinations_count=0;
            return;
        }
        this->reached_destinations_count++;
        route_graph_reset(this->graph);
        this->current_dst = this->destinations->data;
//      route_graph_flood_frugal(this->graph, this->current_dst, this->pos, this->vehicleprofile, this->route_graph_flood_done_cb);
        route_graph_flood_frugal(this->graph, this->current_dst, this->pos, this->vehicleprofile, NULL);
        // route_path_update_done(this,1); // 1 = int new_graph ??? waarschijnlijk niet van doen

    }
}

/**
 * @brief Gets the route_graph_point with the specified coordinates
 *
 * @param this The route in which to search
 * @param c Coordinates to search for
 * @param last The last route graph point returned to iterate over multiple points with the same coordinates
 * @return The point at the specified coordinates or NULL if not found
 */
static struct route_graph_point *
route_graph_get_point_next(struct route_graph *this, struct coord *c, struct route_graph_point *last)
{
    struct route_graph_point *p;
    int seen=0,hashval=HASHCOORD(c);
    p=this->hash[hashval];
    while (p)
    {
        if (p->c.x == c->x && p->c.y == c->y)
        {
            if (!last || seen)
                return p;
            if (p == last)
                seen=1;
        }
        p=p->hash_next;
    }
    return NULL;
}

static struct route_graph_point *
route_graph_get_point(struct route_graph *this, struct coord *c)
{
    return route_graph_get_point_next(this, c, NULL);
}

/**
 * @brief Gets the last route_graph_point with the specified coordinates 
 *
 * @param this The route in which to search
 * @param c Coordinates to search for
 * @return The point at the specified coordinates or NULL if not found
 */
static struct route_graph_point *
route_graph_get_point_last(struct route_graph *this, struct coord *c)
{
    struct route_graph_point *p,*ret=NULL;
    int hashval=HASHCOORD(c);
    p=this->hash[hashval];
    while (p)
    {
        if (p->c.x == c->x && p->c.y == c->y)
            ret=p;
        p=p->hash_next;
    }
    return ret;
}



/**
 * @brief Create a new point for the route graph with the specified coordinates
 *
 * @param this The route to insert the point into
 * @param f The coordinates at which the point should be created
 * @return The point created
 */

static struct route_graph_point *
route_graph_point_new(struct route_graph *this, struct coord *f)
{
    int hashval;
    struct route_graph_point *p;

    hashval=HASHCOORD(f);
    p=g_slice_new0(struct route_graph_point);
    p->hash_next=this->hash[hashval];
    this->hash[hashval]=p;
    p->c=*f;
    return p;
}

/**
 * @brief Inserts a point into the route graph at the specified coordinates
 *
 * This will insert a point into the route graph at the coordinates passed in f.
 * Note that the point is not yet linked to any segments.
 *
 * @param this The route to insert the point into
 * @param f The coordinates at which the point should be inserted
 * @return The point inserted or NULL on failure
 */
static struct route_graph_point *
route_graph_add_point(struct route_graph *this, struct coord *f)
{
    struct route_graph_point *p;

    p=route_graph_get_point(this,f);
    if (!p)
        p=route_graph_point_new(this,f);
    return p;
}

/**
 * @brief Frees all the memory used for points in the route graph passed
 *
 * @param this The route graph to delete all points from
 */
static void
route_graph_free_points(struct route_graph *this)
{
    struct route_graph_point *curr,*next;
    int i;
    for (i = 0 ; i < HASH_SIZE ; i++)
    {
        curr=this->hash[i];
        while (curr)
        {
            next=curr->hash_next;
            g_slice_free(struct route_graph_point, curr);
            curr=next;
        }
        this->hash[i]=NULL;
    }
}

/**
 * @brief Resets all nodes
 *
 * @param this The route graph to reset
 */


static void
route_graph_reset(struct route_graph *this)
{
    struct route_graph_point *curr;
    struct route_graph_segment *seg;
    int i;
    for (i = 0 ; i < HASH_SIZE ; i++)
    {
        curr=this->hash[i];
        while (curr)
        {
            seg = curr->start;
            while (seg)
            {
                seg->end_from_seg = NULL;
                seg->start_from_seg = NULL;
                seg->seg_end_out_cost = INT_MAX;
                seg->seg_start_out_cost = INT_MAX;
                seg = seg->start_next;
            }
            seg = curr->end;
            // overdaad ?
            while (seg)
            {
                seg->end_from_seg = NULL;
                seg->start_from_seg = NULL;
                seg->seg_end_out_cost = INT_MAX;
                seg->seg_start_out_cost = INT_MAX;
                seg = seg->end_next;
            }
            curr=curr->hash_next;
        }
    }
}

/**
 * @brief Returns the position of a certain field appended to a route graph segment
 *
 * This function returns a pointer to a field that is appended to a route graph
 * segment.
 *
 * @param seg The route graph segment the field is appended to
 * @param type Type of the field that should be returned
 * @return A pointer to a field of a certain type, or NULL if no such field is present
 */
static void *
route_segment_data_field_pos(struct route_segment_data *seg, enum attr_type type)
{
    unsigned char *ptr;
    
    ptr = ((unsigned char*)seg) + sizeof(struct route_segment_data);

    if (seg->flags & AF_SPEED_LIMIT) {
        if (type == attr_maxspeed) 
            return (void*)ptr;
        ptr += sizeof(int);
    }
    if (seg->flags & AF_SEGMENTED) {
        if (type == attr_offset) 
            return (void*)ptr;
        ptr += sizeof(int);
    }
    if (seg->flags & AF_SIZE_OR_WEIGHT_LIMIT) {
        if (type == attr_vehicle_width)
            return (void*)ptr;
        ptr += sizeof(struct size_weight_limit);
    }
    if (seg->flags & AF_DANGEROUS_GOODS) {
        if (type == attr_vehicle_dangerous_goods)
            return (void*)ptr;
        ptr += sizeof(int);
    }
    return NULL;
}

/**
 * @brief Calculates the size of a route_segment_data struct with given flags
 *
 * @param flags The flags of the route_segment_data
 */

static int
route_segment_data_size(int flags)
{
    int ret=sizeof(struct route_segment_data);
    if (flags & AF_SPEED_LIMIT)
        ret+=sizeof(int);
    if (flags & AF_SEGMENTED)
        ret+=sizeof(int);
    if (flags & AF_SIZE_OR_WEIGHT_LIMIT)
        ret+=sizeof(struct size_weight_limit);
    if (flags & AF_DANGEROUS_GOODS)
        ret+=sizeof(int);
    return ret;
}


static int
route_graph_segment_is_duplicate(struct route_graph_point *start, struct route_graph_segment_data *data)
{
    struct route_graph_segment *s;
    s=start->start;
    while (s)
    {
        if (item_is_equal(*data->item, s->data.item))
        {
            if (data->flags & AF_SEGMENTED)
            {
                if (RSD_OFFSET(&s->data) == data->offset)
                {
                    return 1;
                }
            }
            else
                return 1;
        }
        s=s->start_next;
    } 
    return 0;
}

/**
 * @brief Inserts a new segment into the route graph
 *
 * This function performs a check if a segment for the item specified already exists, and inserts
 * a new segment representing this item if it does not.
 *
 * fixme : -jandegr- IMHO this does not check if it already exists, contrary to the lines above
 *
 * @param this The route graph to insert the segment into
 * @param start The graph point which should be connected to the start of this segment
 * @param end The graph point which should be connected to the end of this segment
 * @param len The length of this segment  ??????????
 * @param item The item that is represented by this segment  ???????????
 * @param flags Flags for this segment ??????????????
 * @param offset If the item passed in "item" is segmented (i.e. divided into several segments), this indicates the position of this segment within the item ???????
 * @param maxspeed The maximum speed allowed on this segment in km/h. -1 if not known.  ?????
 */
static void
route_graph_add_segment(struct route_graph *this, struct route_graph_point *start,
            struct route_graph_point *end, struct route_graph_segment_data *data)
{
    struct route_graph_segment *s;
    int size;

    size = sizeof(struct route_graph_segment)-sizeof(struct route_segment_data)+route_segment_data_size(data->flags);
    s = g_slice_alloc0(size);
    if (!s) {
        printf("%s:Out of memory\n", __FUNCTION__);
        return;
    }
    s->seg_end_out_cost = INT_MAX;
    s->seg_start_out_cost = INT_MAX;
    s->start=start;
    s->start_next=start->start;
    start->start=s;
    s->end=end;
    s->end_next=end->end;
    end->end=s;
    s->data.len=data->len;
    s->data.item=*data->item;
    s->data.flags=data->flags;
    s->data.angle_start=data->angle_start;
    s->data.angle_end=data->angle_end;
    if (data->flags & AF_SPEED_LIMIT) 
        RSD_MAXSPEED(&s->data)=data->maxspeed;
    if (data->flags & AF_SEGMENTED) 
        RSD_OFFSET(&s->data)=data->offset;
    if (data->flags & AF_SIZE_OR_WEIGHT_LIMIT) 
        RSD_SIZE_WEIGHT(&s->data)=data->size_weight;
    if (data->flags & AF_DANGEROUS_GOODS) 
        RSD_DANGEROUS_GOODS(&s->data)=data->dangerous_goods;

    s->next=this->route_segments;
    this->route_segments=s;
}

/**
 * @brief Gets all the coordinates of an item
 *
 * This will get all the coordinates of the item i and return them in c,
 * up to max coordinates. Additionally it is possible to limit the coordinates
 * returned to all the coordinates of the item between the two coordinates
 * start end end.
 *
 * @important Make sure that whatever c points to has enough memory allocated
 * @important to hold max coordinates!
 *
 * @param i The item to get the coordinates of
 * @param c Pointer to memory allocated for holding the coordinates
 * @param max Maximum number of coordinates to return
 * @param start First coordinate to get
 * @param end Last coordinate to get
 * @return The number of coordinates returned
 */
static int get_item_seg_coords(struct item *i, struct coord *c, int max,
        struct coord *start, struct coord *end)
{
    struct map_rect *mr;
    struct item *item;
    int rc = 0, p = 0;
    struct coord c1;
    mr=map_rect_new(i->map, NULL);
    if (!mr)
        return 0;
    item = map_rect_get_item_byid(mr, i->id_hi, i->id_lo);
    if (item)
    {
        rc = item_coord_get(item, &c1, 1);
        while (rc && (c1.x != start->x || c1.y != start->y))
        {
            rc = item_coord_get(item, &c1, 1);
        }
        while (rc && p < max)
        {
            c[p++] = c1;
            if (c1.x == end->x && c1.y == end->y)
                break;
            rc = item_coord_get(item, &c1, 1);
        }
    }
    map_rect_destroy(mr);
    return p;
}

/**
 * @brief Returns (and removes one segment from a path)
 *
 * edit : does not remove it anymore
 *
 * @param path The path to take the segment from
 * @param item The item whose segment to remove
 * @param offset Offset of the segment within the item to remove. If the item is not segmented this should be 1.
 * @return The segment removed
 */
static struct route_path_segment *
route_extract_segment_from_path(struct route_path *path, struct item *item, int offset)
{
    int soffset;
    struct route_path_segment *sp = NULL, *s;
    s = path->path;
    while (s)
    {
        if (item_is_equal(s->data->item,*item))
        {
            if (s->data->flags & AF_SEGMENTED)
                soffset=RSD_OFFSET(s->data);
            else
                soffset=1;
            if (soffset == offset)
            {
                if (sp)
                {
                    sp->next = s->next;
                    break;
                }
                else
                {
                    path->path = s->next;
                    break;
                }
            }
        }
        sp = s;
        s = s->next;
    }
//  if (s)
//      item_hash_remove(path->path_hash, item);

    // vermoedelijk wordt oldpath dan compleet wel ergens gewist ?

    return s;
}

/**
 * @brief Adds a segment and the end of a path
 *
 * @param this The path to add the segment to
 * @param segment The segment to add
 */
static void
route_path_add_segment(struct route_path *this, struct route_path_segment *segment)
{
    if (!this->path)
        this->path=segment;
    if (this->path_last)
        this->path_last->next=segment;
    this->path_last=segment;
}

/**
 * @brief Adds a two coordinate line to a path
 *
 * This adds a new line to a path, creating a new segment for it.
 *
 * @param this The path to add the item to
 * @param start coordinate to add to the start of the item. If none should be added, make this NULL.
 * @param end coordinate to add to the end of the item. If none should be added, make this NULL.
 * @param len The length of the item
 */
static void
route_path_add_line(struct route_path *this, struct coord *start, struct coord *end, int len)
{
    int ccnt=2;
    struct route_path_segment *segment;
    int seg_size,seg_dat_size;

    dbg(lvl_debug,"line from 0x%x,0x%x-0x%x,0x%x\n", start->x, start->y, end->x, end->y);
    seg_size=sizeof(*segment) + sizeof(struct coord) * ccnt;
        seg_dat_size=sizeof(struct route_segment_data);
        segment=g_malloc0(seg_size + seg_dat_size);
        segment->data=(struct route_segment_data *)((char *)segment+seg_size);
    segment->ncoords=ccnt;
    segment->direction=0;
    segment->c[0]=*start;
    segment->c[1]=*end;
    segment->data->len=len;
    route_path_add_segment(this, segment);
}

/**
 * @brief Inserts a new item into the path
 * 
 * This function does almost the same as "route_path_add_item()", but identifies
 * the item to add by a segment from the route graph. Another difference is that it "copies" the
 * segment from the route graph, i.e. if the item is segmented, only the segment passed in rgs will
 * be added to the route path, not all segments of the item. 
 *
 * The function can be sped up by passing an old path already containing this segment in oldpath - 
 * the segment will then be extracted from this old path. Please note that in this case the direction
 * parameter has no effect.
 *
 * @param this The path to add the item to
 * @param oldpath Old path containing the segment to be added. Speeds up the function, but can be NULL.
 * @param rgs Segment of the route graph that should be "copied" to the route path
 * @param dir Order in which to add the coordinates. See route_path_add_item()
 * @param pos  Information about start point if this is the first segment
 * @param dst  Information about end point if this is the last segment
 */

static int
route_path_add_item_from_graph(struct route_path *this, struct route_path *oldpath, struct route_graph_segment *rgs, int dir, struct route_info *position, struct route_info *dst)
{
    struct route_path_segment *segment=NULL;
    int i, ccnt, extra=0, ret=0;
    struct coord *c,*cd,ca[2048];
    int offset=1;
    int seg_size,seg_dat_size;
    int len=rgs->data.len;
    if (rgs->data.flags & AF_SEGMENTED) 
        offset=RSD_OFFSET(&rgs->data);

    dbg(lvl_debug,"enter (0x%x,0x%x) dir=%d pos=%p dst=%p\n", rgs->data.item.id_hi, rgs->data.item.id_lo, dir, position, dst);
    if (oldpath)
    {
        segment=item_hash_lookup(oldpath->path_hash, &rgs->data.item);

//      if (segment && !(dst && segment->direction != dir))
        if (segment && this->updated) // try to fix most gap in route as well
        {                             // to be merged into Zanavi after some testing
                                      // will leave the small gap in case of a small pullback
                                      // on the same segment to not make navigation.c reprocess
                                      // the entire route in such cases
            segment = route_extract_segment_from_path(oldpath, &rgs->data.item, offset);
            if (segment)
            {
                ret=1;   // preserve the updated status
                if (!position)
                {
                    segment->data->len=len;
                    segment->next=NULL;
                    item_hash_insert(this->path_hash,  &rgs->data.item, segment);
                    route_path_add_segment(this, segment);
                    return ret;
                }
            }
        }
    }

    if (position)
    {
        if (dst)
        {
            extra=2;
            if (dst->lenneg >= position->lenneg)
            {
                dir=1;
                ccnt=dst->pos-position->pos;
                c=position->street->c+position->pos+1;
                len=dst->lenneg-position->lenneg;
            }
            else
            {
                dir=-1;
                ccnt=position->pos-dst->pos;
                c=position->street->c+dst->pos+1;
                len=position->lenneg-dst->lenneg;
            }
        }
        else
        {
            extra=1;
            dbg(lvl_debug,"pos dir=%d\n", dir);
            dbg(lvl_debug,"pos pos=%d\n", position->pos);
            dbg(lvl_debug,"pos count=%d\n", position->street->count);
            if (dir > 0)
            {
                c=position->street->c+position->pos+1;
                ccnt=position->street->count-position->pos-1;
                len=position->lenpos;
            }
            else
            {
                c=position->street->c;
                ccnt=position->pos+1;
                len=position->lenneg;
            }
        }
        position->dir=dir;
    }
    else if (dst)
    {
        extra=1;
        dbg(lvl_debug,"dst dir=%d\n", dir);
        dbg(lvl_debug,"dst pos=%d\n", dst->pos);
        if (dir > 0) {
            c=dst->street->c;
            ccnt=dst->pos+1;
            len=dst->lenneg;
        }
        else
        {
            c=dst->street->c+dst->pos+1;
            ccnt=dst->street->count-dst->pos-1;
            len=dst->lenpos;
        }
    }
    else
    {
        ccnt=get_item_seg_coords(&rgs->data.item, ca, 2047, &rgs->start->c, &rgs->end->c);
        c=ca;
    }
    seg_size=sizeof(*segment) + sizeof(struct coord) * (ccnt + extra);
    seg_dat_size=route_segment_data_size(rgs->data.flags);
    segment=g_malloc0(seg_size + seg_dat_size);
    segment->data=(struct route_segment_data *)((char *)segment+seg_size);
    segment->direction=dir; //
    cd=segment->c;
    if (position && (c[0].x != position->lp.x || c[0].y != position->lp.y))
        *cd++=position->lp;
    if (dir < 0)
        c+=ccnt-1;
    for (i = 0 ; i < ccnt ; i++)
    {
        *cd++=*c;
        c+=dir; 
    }
    segment->ncoords+=ccnt;
    if (dst && (cd[-1].x != dst->lp.x || cd[-1].y != dst->lp.y)) 
        *cd++=dst->lp;
    segment->ncoords=cd-segment->c;
    if (segment->ncoords <= 1)
    {
        g_free(segment);
        return 1;
    }

    /* We check if the route graph segment is part of a roundabout here, because this
     * only matters for route graph segments which form parts of the route path */

// no need for OSM
//  if (!(rgs->data.flags & AF_ROUNDABOUT)) {
        // We identified this roundabout earlier
//      route_check_roundabout(rgs, 13, (dir < 1), NULL);
//  }

    memcpy(segment->data, &rgs->data, seg_dat_size);
    segment->data->len=len;
    segment->next=NULL;
    item_hash_insert(this->path_hash,  &rgs->data.item, segment);

    route_path_add_segment(this, segment);

    return ret;
}

/**
 * @brief Destroys all segments of a route graph
 *
 * @param this The graph to destroy all segments from
 */
static void
route_graph_free_segments(struct route_graph *this)
{
    struct route_graph_segment *curr,*next;
    int size;
    curr=this->route_segments;
    while (curr)
    {
        next=curr->next;
        size = sizeof(struct route_graph_segment)-sizeof(struct route_segment_data)+route_segment_data_size(curr->data.flags);
        g_slice_free1(size, curr);
        curr=next;
    }
    this->route_segments=NULL;
}

/**
 * @brief Destroys a route graph
 * 
 * @param this The route graph to be destroyed
 */
static void
route_graph_destroy(struct route_graph *this)
{
    dbg(0,"free Graph enter\n");
    if (this)
    {
        dbg(0,"free Graph\n");
        route_graph_build_done(this, 1);
        route_graph_free_points(this);
        route_graph_free_segments(this);
        g_free(this);
    }
}

/**
 * @brief Returns the estimated speed on a segment
 *
 * This function returns the estimated speed to be driven on a segment, 0=not passable
 * Uses route_weight, roundabout_weight and link_weight from the roadprofile in navit.xml to
 * modify the speed or OSM maxspeed
 *
 *
 * @param profile The routing preferences
 * @param over The segment which is passed
 * @param dist A traffic distortion if applicable
 * @return The estimated speed
 */
static int
route_seg_speed(struct vehicleprofile *profile, struct route_segment_data *over, struct route_traffic_distortion *dist)
{
    struct roadprofile *roadprofile=vehicleprofile_get_roadprofile(profile, over->item.type);
    int speed,maxspeed;
    int route_averaging = 100;
    if (!roadprofile || !roadprofile->speed)
            return 0;
    /* maxspeed_handling: 0=always, 1 only if maxspeed restricts the speed,
     *  2 never
     *  */


    if (roadprofile->route_weight)
        route_averaging =roadprofile->route_weight;

    if ((over->flags & AF_ROUNDABOUT) && roadprofile->roundabout_weight)
        route_averaging = (route_averaging * roadprofile->roundabout_weight)/100;
    else
        if ((over->flags & AF_LINK) && roadprofile->link_weight)
        {
            route_averaging = (route_averaging * roadprofile->link_weight)/100;
        }
    speed=(roadprofile->speed * route_averaging)/100 ;
    if (profile->maxspeed_handling != 2)
    {
        if (over->flags & AF_SPEED_LIMIT)
        {
            maxspeed=RSD_MAXSPEED(over);
            if (!profile->maxspeed_handling) /*always handle maxspeed*/
            {
                if (roadprofile->route_weight)
                {
                    route_averaging = 100;
                    if (maxspeed > (roadprofile->speed * 0.7) && (maxspeed < roadprofile->speed))
                    {
                        route_averaging =
                            (((roadprofile->route_weight -100) * (maxspeed - (roadprofile->speed * 0.7)))
                            /(30 * roadprofile->speed))
                            + 100;
                    }
                    else if (maxspeed >= roadprofile->speed)
                    {
                        route_averaging =roadprofile->route_weight;
                    }
                    maxspeed=(maxspeed * route_averaging)/100 ;
                }

                if ((over->flags & AF_ROUNDABOUT) && roadprofile->roundabout_weight)
                        maxspeed = (maxspeed * roadprofile->roundabout_weight)/100;
                else if ((over->flags & AF_LINK) && roadprofile->link_weight)
                    {
                        maxspeed = (maxspeed * roadprofile->link_weight)/100;
                    }
            }
        }
        else
            maxspeed=INT_MAX;

        if (dist && maxspeed > dist->maxspeed)
            maxspeed=dist->maxspeed;

        /*below handling 0=always or 1=restricting */
        if (maxspeed != INT_MAX && (profile->maxspeed_handling == 0 || maxspeed < speed))
            speed=maxspeed;
    }
    else /* handling=2, don't use maxspeed*/
        speed=(roadprofile->speed * route_averaging)/100 ;
    if (over->flags & AF_DANGEROUS_GOODS)
    {
        if (profile->dangerous_goods & RSD_DANGEROUS_GOODS(over))
            return 0;
    }
    /*verkeerde plaats om dit te checken*/
    if (over->flags & AF_SIZE_OR_WEIGHT_LIMIT)
    {
        struct size_weight_limit *size_weight=&RSD_SIZE_WEIGHT(over);
        if (size_weight->width != -1 && profile->width != -1 && profile->width > size_weight->width)
            return 0;
        if (size_weight->height != -1 && profile->height != -1 && profile->height > size_weight->height)
            return 0;
        if (size_weight->length != -1 && profile->length != -1 && profile->length > size_weight->length)
            return 0;
        if (size_weight->weight != -1 && profile->weight != -1 && profile->weight > size_weight->weight)
            return 0;
        if (size_weight->axle_weight != -1 && profile->axle_weight != -1 && profile->axle_weight > size_weight->axle_weight)
            return 0;
    }
    if (speed > heuristic_speed) /*for A_star*/
        return heuristic_speed;
    return speed;
}

/**
 * @brief Returns the time needed to drive len on item
 *
 * This function returns the time needed to drive len meters on 
 * the item passed in item in tenth of seconds.
 *
 * @param profile The routing preferences
 * @param over The segment which is passed
 * @param dist A traffic distortion if applicable
 * @return The time needed to drive len on item in thenth of senconds
 */

static int
route_time_seg(struct vehicleprofile *profile, struct route_segment_data *over, struct route_traffic_distortion *dist)
{
    int speed=route_seg_speed(profile, over, dist);
    if (!speed)
        return INT_MAX;
    return over->len*36/speed+(dist ? dist->delay : 0);
}

static int
route_get_traffic_distortion(struct route_graph_segment *seg, struct route_traffic_distortion *ret)
{
    struct route_graph_point *start=seg->start;
    struct route_graph_point *end=seg->end;
    struct route_graph_segment *tmp,*found=NULL;
    tmp=start->start;
    while (tmp && !found) {
        if (tmp->data.item.type == type_traffic_distortion && tmp->start == start && tmp->end == end)
            found=tmp;
        tmp=tmp->start_next;
    }
    tmp=start->end;
    while (tmp && !found) {
        if (tmp->data.item.type == type_traffic_distortion && tmp->end == start && tmp->start == end) 
            found=tmp;
        tmp=tmp->end_next;
    }
    if (found) {
        ret->delay=found->data.len;
        if (found->data.flags & AF_SPEED_LIMIT)
            ret->maxspeed=RSD_MAXSPEED(&found->data);
        else
            ret->maxspeed=INT_MAX;
        return 1;
    }
    return 0;
}

static int
route_through_traffic_allowed(struct vehicleprofile *profile, struct route_graph_segment *seg)
{
    return (seg->data.flags & AF_THROUGH_TRAFFIC_LIMIT) == 0;
}

/**
 * @brief Returns the "costs" of driving from segment from over segment over in direction dir
 *
 * @param profile The routing preferences
 * @param from The segment we come from
 * @param over The segment we are using
 * @param dir The direction of segment which we are driving
 * @return The "costs" needed to drive len on item
 */  

static int
route_value_seg(struct vehicleprofile *profile, struct route_graph_segment *from, struct route_graph_segment *over, int dir)
{
    int ret;
    int delta=0;
    struct route_traffic_distortion dist,*distp=NULL;
    dbg(lvl_debug,"flags 0x%x mask 0x%x flags 0x%x\n", over->data.flags, dir >= 0 ? profile->flags_forward_mask : profile->flags_reverse_mask, profile->flags);
    if ((over->data.flags & (dir >= 0 ? profile->flags_forward_mask : profile->flags_reverse_mask)) != profile->flags)
        return INT_MAX;
//  if (dir > 0 && (over->start->flags & RP_TURN_RESTRICTION))
//      return INT_MAX;
//  if (dir < 0 && (over->end->flags & RP_TURN_RESTRICTION))
//      return INT_MAX;
    if (dir > 0 && from && (over->end->flags & RP_TURN_RESTRICTION))
    {
        if (!is_turn_allowed(over->end,over,from))
        {
            return INT_MAX;
        }
    }
    if (dir < 0 && from && (over->start->flags & RP_TURN_RESTRICTION))
    {
        if (!is_turn_allowed(over->start,over,from))
        {
            return INT_MAX;
        }
    }

    if (from && from == over)
        return INT_MAX;
    if ((over->start->flags & RP_TRAFFIC_DISTORTION) && (over->end->flags & RP_TRAFFIC_DISTORTION) && 
        route_get_traffic_distortion(over, &dist) && dir != 2 && dir != -2)
    {
            distp=&dist;
    }
    if (profile->mode != 3)/*not new shortest*/
    {
        ret=route_time_seg(profile, &over->data, distp);

        if (from)
        {
            if (from->end == over->end)
            {
                delta= from->data.angle_end - over->data.angle_end + 180;
            }
            else if (from->start == over->end)
            {
                delta= from->data.angle_start - over->data.angle_end;
    //          dbg(0,"SEG_BACKWARD dir positief, delta=%i, over_angle_end=%i, seg_angle_end=%i\n",delta,over->data.angle_end,from->data.angle_end);
            }
            else if (from->end == over->start)
            {
                delta= from->data.angle_end - over->data.angle_start;
    //          dbg(0,"SEG_FOR dir negatief, delta=%i, from_angle_end=%i, over_angle_start=%i\n",delta,from->data.angle_end,over->data.angle_start);
            }
            else if (from->start == over->start)
            {
                delta= from->data.angle_start - over->data.angle_start - 180;
    //          dbg(0,"SEG_BACK dir negatief, delta=%i, over_angle_start=%i, from_angle_start=%i\n",delta,over->data.angle_start,from->data.angle_start);
            }

            if (delta < -180)
                delta+=360;
            if (delta > 180)
                delta-=360;
            if (abs(delta) > 110)
            {
            /*add 2 tenths of a  second per degree above threshold (110 degr.)*/
                ret=ret+abs(delta-110)+abs(delta-110);
    //          dbg(0,"from=%s, over=%s\n",item_to_name(from->data.item.type),item_to_name(over->data.item.type));
    //          dbg(0,"dir =%i, added %i tenths of seconds, cost=%i, delta=%i\n",dir,(abs(delta-110)*2),ret,delta);
            }
        }
    }
    else ret = over->data.len; /*new shortest mode*/

    if (ret == INT_MAX)
        return ret;
    if (!route_through_traffic_allowed(profile, over) && from && route_through_traffic_allowed(profile, from))
        ret+=profile->through_traffic_penalty;
    return ret;
}

static int
route_graph_segment_match(struct route_graph_segment *s1, struct route_graph_segment *s2)
{
    if (!s1 || !s2)
        return 0;
    return (s1->start->c.x == s2->start->c.x && s1->start->c.y == s2->start->c.y && 
        s1->end->c.x == s2->end->c.x && s1->end->c.y == s2->end->c.y);
}

static void
route_graph_set_traffic_distortion(struct route_graph *this, struct route_graph_segment *seg, int delay)
{
    struct route_graph_point *start=NULL;
    struct route_graph_segment *s;

    while ((start=route_graph_get_point_next(this, &seg->start->c, start))) {
        s=start->start;
        while (s) {
            if (route_graph_segment_match(s, seg)) {
                if (s->data.item.type != type_none && s->data.item.type != type_traffic_distortion && delay) {
                    struct route_graph_segment_data data;
                    struct item item;
                    memset(&data, 0, sizeof(data));
                    memset(&item, 0, sizeof(item));
                    item.type=type_traffic_distortion;
                    data.item=&item;
                    data.len=delay;
                    s->start->flags |= RP_TRAFFIC_DISTORTION;
                    s->end->flags |= RP_TRAFFIC_DISTORTION;
                    route_graph_add_segment(this, s->start, s->end, &data);
                } else if (s->data.item.type == type_traffic_distortion && !delay) {
                    s->data.item.type = type_none;
                }
            }
            s=s->start_next;
        }
    }
}

/**
 * @brief Adds a route distortion item to the route graph
 *
 * @param this The route graph to add to
 * @param item The item to add
 */
static void
route_process_traffic_distortion(struct route_graph *this, struct item *item)
{
    struct route_graph_point *s_pnt,*e_pnt;
    struct coord c,l;
    struct attr delay_attr, maxspeed_attr;
    struct route_graph_segment_data data;

    data.item=item;
    data.len=0;
    data.flags=0;
    data.offset=1;
    data.maxspeed = INT_MAX;

    if (item_coord_get(item, &l, 1)) {
        s_pnt=route_graph_add_point(this,&l);
        while (item_coord_get(item, &c, 1)) {
            l=c;
        }
        e_pnt=route_graph_add_point(this,&l);
        s_pnt->flags |= RP_TRAFFIC_DISTORTION;
        e_pnt->flags |= RP_TRAFFIC_DISTORTION;
        if (item_attr_get(item, attr_maxspeed, &maxspeed_attr)) {
            data.flags |= AF_SPEED_LIMIT;
            data.maxspeed=maxspeed_attr.u.num;
        }
        if (item_attr_get(item, attr_delay, &delay_attr))
            data.len=delay_attr.u.num;
        route_graph_add_segment(this, s_pnt, e_pnt, &data);
    }
}

/**
 * @brief Adds a turn restriction item to the route graph
 *
 * @param this The route graph to add to
 * @param item The item to add
 */
static void
route_graph_add_turn_restriction(struct route_graph *this, struct item *item)
{
    struct route_graph_point *pnt[4];
    struct coord c[5];
    int i,count;
    struct route_graph_segment_data data;

    count=item_coord_get(item, c, 5);
    if (count != 3 && count != 4)
    {
        dbg(lvl_debug,"wrong count %d\n",count);
        return;
    }
    if (count == 4)
        return;
    for (i = 0 ; i < count ; i++) 
        // bestaand punt of een nieuw indien nodig
        pnt[i]=route_graph_add_point(this,&c[i]);
    dbg(lvl_debug,"%s: (0x%x,0x%x)-(0x%x,0x%x)-(0x%x,0x%x) %p-%p-%p count=%i\n",item_to_name(item->type),c[0].x,c[0].y,c[1].x,c[1].y,c[2].x,c[2].y,pnt[0],pnt[1],pnt[2],count);
    data.item=item;
    data.flags=0;
    data.len=0;
    route_graph_add_segment(this, pnt[0], pnt[1], &data);
    route_graph_add_segment(this, pnt[1], pnt[2], &data);
#if 0
    if (count == 4)
    {
        pnt[1]->flags |= RP_TURN_RESTRICTION;
        pnt[2]->flags |= RP_TURN_RESTRICTION;
        route_graph_add_segment(this, pnt[2], pnt[3], &data);
    }
    else
#endif
        pnt[1]->flags |= RP_TURN_RESTRICTION;
}

/**
 * @brief Adds an item to the route graph
 *
 * This adds an item (e.g. a street) to the route graph, creating as many segments as needed for a
 * segmented item.
 *
 * @param this The route graph to add to
 * @param item The item to add
 * @param profile       The vehicle profile currently in use
 */
static void
route_graph_add_item(struct route_graph *this, struct item *item, struct vehicleprofile *profile)
{
#ifdef AVOID_FLOAT
    int len=0;
#else
    double len=0;
#endif
    int segmented = 0;
    struct roadprofile *roadp;
    struct route_graph_point *s_pnt,*e_pnt;
    struct coord c,l;
    struct attr attr;
    struct route_graph_segment_data data;
    data.flags=0;
    data.offset=1;
    data.maxspeed=-1;
    data.item=item;

    roadp = vehicleprofile_get_roadprofile(profile, item->type);
    if (!roadp)
    {
        // Don't include any roads that don't have a road profile in our vehicle profile
        return;
    }

    if (item_coord_get(item, &l, 1))
    {
        int default_flags_value=AF_ALL;
        int *default_flags=item_get_default_flags(item->type);
        if (! default_flags)
            default_flags=&default_flags_value;
        if (item_attr_get(item, attr_flags, &attr))
        {
            data.flags = attr.u.num;
            if (data.flags & AF_SEGMENTED)
                segmented = 1;
        }
        else
            data.flags = *default_flags;

    }
    if (data.flags & AF_SPEED_LIMIT)
    {
            if (item_attr_get(item, attr_maxspeed, &attr)) 
                data.maxspeed = attr.u.num;
    }
    if (data.flags & AF_DANGEROUS_GOODS)
    {
        if (item_attr_get(item, attr_vehicle_dangerous_goods, &attr))
            data.dangerous_goods = attr.u.num;
        else
            data.flags &= ~AF_DANGEROUS_GOODS;
    }

    if (data.flags & AF_SIZE_OR_WEIGHT_LIMIT)
    {
        if (item_attr_get(item, attr_vehicle_width, &attr))
            data.size_weight.width=attr.u.num;
        else
            data.size_weight.width=-1;
        if (item_attr_get(item, attr_vehicle_height, &attr))
            data.size_weight.height=attr.u.num;
        else
            data.size_weight.height=-1;
        if (item_attr_get(item, attr_vehicle_length, &attr))
            data.size_weight.length=attr.u.num;
        else
            data.size_weight.length=-1;
        if (item_attr_get(item, attr_vehicle_weight, &attr))
            data.size_weight.weight=attr.u.num;
        else
            data.size_weight.weight=-1;
        if (item_attr_get(item, attr_vehicle_axle_weight, &attr))
            data.size_weight.axle_weight=attr.u.num;
        else
            data.size_weight.axle_weight=-1;
    }
        /* eerste punt is hier blijkbaar reeds gekend */
        /* maar geen enkele controle */
    s_pnt=route_graph_add_point(this,&l);

    if (!segmented)
    {
        if (item_coord_get(item, &c, 1))
        {

            data.angle_start=transform_get_angle_delta(&l,&c,1);
            dbg(lvl_debug,"angle_start=%i, l.x=%i, l.y=%i, c.x=%i, c.y=%i\n",data.angle_start,(&l)->x,(&l)->y,(&c)->x,(&c)->y);

            while (1)
            {
                data.angle_end=transform_get_angle_delta(&l,&c,1);
                len+=transform_distance(map_projection(item->map), &l, &c);
                l=c;
                if (!item_coord_get(item, &c, 1))
                    break;
            }
            dbg(lvl_debug,"angle_end=%i\n",data.angle_end);
        }

        e_pnt=route_graph_add_point(this,&l);
        dbg_assert(len >= 0);
        data.len=len;
        if (!route_graph_segment_is_duplicate(s_pnt, &data))
            route_graph_add_segment(this, s_pnt, e_pnt, &data);
    }
    else
    {
        int isseg,rc;
        int sc = 0;
        do
        {
            isseg = item_coord_is_node(item);
            rc = item_coord_get(item, &c, 1);
            if (rc)
            {
                len+=transform_distance(map_projection(item->map), &l, &c);
                l=c;
                if (isseg)
                {
                    e_pnt=route_graph_add_point(this,&l);
                    data.len=len;
                    if (!route_graph_segment_is_duplicate(s_pnt, &data))
                        route_graph_add_segment(this, s_pnt, e_pnt, &data);
                    data.offset++;
                    s_pnt=route_graph_add_point(this,&l);
                    len = 0;
                }
            }
        }
        while(rc);

        e_pnt=route_graph_add_point(this,&l);
        dbg_assert(len >= 0);
        sc++;
        data.len=len;
        if (!route_graph_segment_is_duplicate(s_pnt, &data))
            route_graph_add_segment(this, s_pnt, e_pnt, &data);
    }

}

static struct route_graph_segment *
route_graph_get_segment(struct route_graph *graph, struct street_data *sd, struct route_graph_segment *last)
{
    struct route_graph_point *start=NULL;
    struct route_graph_segment *s;
    int seen=0;

    while ((start=route_graph_get_point_next(graph, &sd->c[0], start))) {
        s=start->start;
        while (s) {
            if (item_is_equal(sd->item, s->data.item)) {
                if (!last || seen)
                    return s;
                if (last == s)
                    seen=1;
            }
            s=s->start_next;
        }
    }
    return NULL;
}


/**
 * @brief Calculates the route with the least cost.
 *
 * It assigns each point a segment one should follow from that point on to reach
 * the destination at the stated costs.
 * 
 * This function uses dijkstra or A* algorithm to do the routing.
 * A* will probably never get really used, but I absolutely wanted to test
 *
 */
static void
route_graph_flood_frugal(struct route_graph *this, struct route_info *dst, struct route_info *pos, struct vehicleprofile *profile, struct callback *cb)
{
    struct route_graph_point *p_min;
    struct route_graph_segment *s=NULL;
    struct route_graph_segment *pos_segment=NULL;
    struct route_graph_segment *s_min=NULL;
    int min,new,val;
    struct fibheap *heap; /* This heap will hold segments with "temporarily" calculated costs */
    int edges_count=0;
    int max_cost= INT_MAX;
    int estimate= INT_MAX;
    int A_star = 0;         /*0=dijkstra, 1=A*/


    heuristic_speed = 130; // in km/h

//  double timestamp_graph_flood = now_ms();

    if (!A_star)
    {
        dbg(lvl_debug,"starting route_graph_flood_frugal DIJKSTRA\n");
    }
    else
    {
        dbg(lvl_debug,"starting route_graph_flood_frugal A_STAR\n");
    }

    pos_segment=route_graph_get_segment(this, pos->street, pos_segment);

    heap = fh_makekeyheap();

    while ((s=route_graph_get_segment(this, dst->street, s)))
    {
        val=route_value_seg(profile, NULL, s, -1);
        if (val != INT_MAX)
        {
            val=val*(100-dst->percent)/100;
            s->seg_end_out_cost = val;
            if (A_star)
            {
                if (profile->mode != 3) // not new shortest
                {
                    estimate = val + ((int)transform_distance(projection_mg, (&(s->end->c)), (&(pos->c))))*36/heuristic_speed;
                }
                else
                {
                    estimate = val + (int)transform_distance(projection_mg, (&(s->end->c)), (&(pos->c)));
                }
            }
            else
                estimate = val;
            s->el_end=fh_insertkey(heap, estimate, s);
        }
        dbg(0,"check destination segment end , val =%i, est_time = %i\n",val,estimate);
        val=route_value_seg(profile, NULL, s, 1);
        if (val != INT_MAX)
        {
            val=val*dst->percent/100;
            s->seg_start_out_cost = val;
            if (A_star)
            {
                if (profile->mode != 3) // not new shortest
                {
                    estimate = val + ((int)transform_distance(projection_mg, (&(s->start->c)), (&(pos->c))))*36/heuristic_speed;
                }
                else
                {
                    estimate = val + (int)transform_distance(projection_mg, (&(s->start->c)), (&(pos->c)));
                }
            }
            else
                estimate = val;
            s->el_start=fh_insertkey(heap, estimate, s);
        }
        dbg(0,"check destination segment start , val =%i, est_time =%i\n",val,estimate);
    }

    for (;;)
    {
        s_min=fh_extractmin(heap);
        if (! s_min)
            break;

        if (s_min->el_end)
        {
            min=s_min->seg_end_out_cost;
            if (s_min->el_start && (s_min->seg_start_out_cost < min))
            {
                min=s_min->seg_start_out_cost;
                p_min = s_min->start;
                s_min->el_start = NULL;
            }
            else
            {
                p_min = s_min->end;
                s_min->el_end = NULL;
            }
        }
        else
        {
            min=s_min->seg_start_out_cost;
            p_min = s_min->start;
            s_min->el_start = NULL;
        }

        // en wat als ze gelijk zijn ???

        s=p_min->start;
        while (s)
        { /* Iterating all the segments leading away from our point to update the points at their ends */
            edges_count ++;
            val=route_value_seg(profile, s_min, s, -1);
            if (val != INT_MAX)
            {
                new=min+val;
                if (new < s->seg_end_out_cost)
                {
                    s->seg_end_out_cost=new;
                    s->start_from_seg=s_min;
                    if (A_star)
                    {
                        if (profile->mode != 3) // not new shortest
                        {
                            estimate = new + ((int)transform_distance(projection_mg, (&(s->end->c)), (&(pos->c))))*36/heuristic_speed;
                        }
                        else
                        {
                            estimate = new + (int)transform_distance(projection_mg, (&(s->end->c)), (&(pos->c)));
                        }
                    }
                    else
                        estimate = new;
                    if (! s->el_end)
                    {
                        s->el_end=fh_insertkey(heap, estimate, s);
                    }
                    else
                    {
                        fh_replacekey(heap, s->el_end, estimate);
                    }
                }
                if (item_is_equal(pos_segment->data.item,s->data.item))
                {   // todo : calculate both ends of pos_segment if using A* ?
                    max_cost=new;
                    dbg(0,"new shortest path cost via end_out= %i\n",new);
                    dbg(0,"number of edges visited =%i\n",edges_count);
                }
            }
            s=s->start_next;
        }

        s=p_min->end;
        while (s)
        { /* Doing the same as above with the segments leading towards our point */
            edges_count ++;
            val=route_value_seg(profile, s_min, s, 1);
            if (val != INT_MAX)
            {
                new=min+val;
                if (new < s->seg_start_out_cost)
                {
                    s->seg_start_out_cost=new;
                    s->end_from_seg=s_min;
                    if (A_star)
                        if (profile->mode != 3) // not new shortest
                        {
                            estimate = new + ((int)transform_distance(projection_mg, (&(s->start->c)), (&(pos->c))))*36/heuristic_speed;
                        }
                        else
                        {
                            estimate = new + (int)transform_distance(projection_mg, (&(s->start->c)), (&(pos->c)));
                        }
                    else
                        estimate = new;
                    if (! s->el_start)
                    {
                        s->el_start=fh_insertkey(heap, estimate, s);
                    }
                    else
                    {
                        fh_replacekey(heap, s->el_start, estimate);
                    }
                }
                if (item_is_equal(pos_segment->data.item,s->data.item))
                {   // todo : calculate both ends of pos_segment if using A* ?
                    max_cost=new;
                    dbg(0,"new shortest path cost via start_out= %i\n",new);
                    dbg(0,"number of edges visited =%i\n",edges_count);
                }
            }
            s=s->end_next;
        }
        if (A_star && !(max_cost == INT_MAX))
            break;
    }
    dbg(0,"number of edges visited =%i\n",edges_count);
//  dbg(0,"route_graph_flood FRUGAL took: %.3f ms\n", now_ms() - timestamp_graph_flood);
    fh_deleteheap(heap);
    if (cb)
    {
        callback_call_0(cb);
    }
    else
    {
        dbg (0,"NO callBack\n");
    }
}


/**
 * @brief Starts an "offroad" path
 *
 * This starts a path that is not located on a street. It creates a new route path
 * adding only one segment, that leads from pos to dest, and which is not associated with an item.
 *
 * @param this Not used
 * @param pos The starting position for the new path
 * @param dst The destination of the new path
 * @param dir Not used
 * @return The new path
 */
static struct route_path *
route_path_new_offroad(struct route_graph *this, struct route_info *pos, struct route_info *dst)
{
    struct route_path *ret;

    ret=g_new0(struct route_path, 1);
    ret->in_use=1;
    ret->path_hash=item_hash_new();
    route_path_add_line(ret, &pos->c, &dst->c, pos->lenextra+dst->lenextra);
    ret->updated=1;

    return ret;
}

/**
 * @brief Returns a coordinate at a given distance
 *
 * This function returns the coordinate, where the user will be if he
 * follows the current route for a certain distance.
 *
 * @param this_ The route we're driving upon
 * @param dist The distance in meters
 * @return The coordinate where the user will be in that distance
 */
struct coord 
route_get_coord_dist(struct route *this_, int dist)
{
    int d,l,i,len;
    int dx,dy;
    double frac;
    struct route_path_segment *cur;
    struct coord ret;
    enum projection pro=route_projection(this_);
    struct route_info *dst=route_get_dst(this_);

    d = dist;

    if (!this_->path2 || pro == projection_none) {
        return this_->pos->c;
    }
    
    ret = this_->pos->c;
    cur = this_->path2->path;
    while (cur) {
        if (cur->data->len < d) {
            d -= cur->data->len;
        } else {
            for (i=0; i < (cur->ncoords-1); i++) {
                l = d;
                len = (int)transform_polyline_length(pro, (cur->c + i), 2);
                d -= len;
                if (d <= 0) { 
                    // We interpolate a bit here...
                    frac = (double)l / len;
                    
                    dx = (cur->c + i + 1)->x - (cur->c + i)->x;
                    dy = (cur->c + i + 1)->y - (cur->c + i)->y;
                    
                    ret.x = (cur->c + i)->x + (frac * dx);
                    ret.y = (cur->c + i)->y + (frac * dy);
                    return ret;
                }
            }
            return cur->c[(cur->ncoords-1)];
        }
        cur = cur->next;
    }

    return dst->c;
}

/**
 * @brief Creates a new route path
 * 
 * This creates a new non-trivial route. It therefore needs the routing information created by route_graph_flood, so
 * make sure to run route_graph_flood() after changing the destination before using this function.
 *
 * @param this The route graph to create the route from
 * @param oldpath (Optional) old path which may contain parts of the new part - this speeds things up a bit. May be NULL.
 * @param pos The starting position of the route
 * @param dst The destination of the route
 * @param preferences The routing preferences
 * @return The new route path
 */
static struct route_path *
route_path_new(struct route_graph *this, struct route_path *oldpath, struct route_info *pos, struct route_info *dst, struct vehicleprofile *profile)
{
    struct route_graph_segment *s=NULL,*s1=NULL,*s2=NULL;
    struct route_graph_point *start;
    struct route_info *posinfo, *dstinfo;
    int segs=0,dir;
    int val1=INT_MAX,val2=INT_MAX;
    int val,val1_new,val2_new;
    struct route_path *ret;

    if (! pos->street || ! dst->street) {
        dbg(lvl_error,"pos or dest not set\n");
        return NULL;
    }

    // street direction from tracking
    dbg(lvl_debug,"street_direction = %i\n",pos->street_direction);


    if (profile->mode == 2 || (profile->mode == 0 && pos->lenextra + dst->lenextra > transform_distance(map_projection(pos->street->item.map), &pos->c, &dst->c)))
        return route_path_new_offroad(this, pos, dst);
    if (profile->mode != 3) /*not shortest on-road*/
    {
        while ((s=route_graph_get_segment(this, pos->street, s)))
        {
            dbg(lvl_debug,"seg_start_out_cost = %i\n",s->seg_start_out_cost);
            val=route_value_seg(profile, NULL, s, 2);
            dbg(lvl_debug,"position street value forward =%i\n",val);
            if (val != INT_MAX && s->seg_start_out_cost != INT_MAX)
            {
                val=val*(pos->percent)/100; // cost om naar het andere uiteinde te rijden !!
            //  dbg(0,"val %d\n",val);
//              if (route_graph_segment_match(s,this->avoid_seg) && pos->street_direction < 0)
//                  val+=profile->turn_around_penalty;
                dbg(lvl_debug,"seg_start_out_cost = %i, val = %d\n",s->seg_start_out_cost,val);
                val1_new=s->seg_start_out_cost - val;
                if (pos->street_direction == -1)
                {
                    val1_new = val1_new + profile->turn_around_penalty;
                    dbg(lvl_debug,"added %i to cost via start_out\n",profile->turn_around_penalty);
                }
                dbg(lvl_debug,"%d - val = %d\n",s->seg_start_out_cost,val1_new);
                if (val1_new < val1)
                {
                    val1=val1_new;
                    s1=s;
                }
            }
            dbg(lvl_debug,"seg_end_out_cost = %i\n",s->seg_end_out_cost);
            val=route_value_seg(profile, NULL, s, -2);
            dbg(lvl_debug,"position street value backward =%i\n",val);
            if (val != INT_MAX && s->seg_end_out_cost != INT_MAX)
            {
                val=val*(100-pos->percent)/100;
                dbg(lvl_debug,"val2 %d\n",val);
//              if (route_graph_segment_match(s,this->avoid_seg) && pos->street_direction > 0)
//                  val+=profile->turn_around_penalty;
                dbg(lvl_debug,"seg_end_out_cost = %i, val = %d\n",s->seg_end_out_cost,val);
                val2_new=s->seg_end_out_cost - val;

                if (pos->street_direction == 1)
                {
                    val2_new = val2_new + profile->turn_around_penalty;
                    dbg(lvl_debug,"added %i to cost via end_out\n",profile->turn_around_penalty);
                }

                dbg(lvl_debug,"%d - val2 = %d\n",s->seg_end_out_cost,val2_new);
                if (val2_new < val2)
                {
                    val2=val2_new;
                    s2=s;
                }
            }
        }
    }
    else /*experimental shortest route*/
    {
        dbg(lvl_debug,"experimental shortest route calc\n")
        while ((s=route_graph_get_segment(this, pos->street, s)))
        {
            val = s->data.len;
            if (val != INT_MAX && s->seg_start_out_cost != INT_MAX  )
            {
                val=val*(100-pos->percent)/100;
                dbg(lvl_debug,"val1 %d\n",val);
                val1_new=s->seg_start_out_cost - val;
    //          dbg(lvl_debug,"val1 +%d=%d\n",s->end->value,val1_new);
                if (val1_new < val1)
                {
                    val1=val1_new;
                    s1=s;
                }
            }
            dbg(lvl_debug,"val=%i\n",val);
            val = s->data.len;
            if (val != INT_MAX  && s->seg_end_out_cost != INT_MAX)
            {
                val=val*pos->percent/100;
                dbg(lvl_debug,"val2 %d\n",val);
                val2_new=s->seg_end_out_cost - val;
    //          dbg(lvl_debug,"val2 +%d=%d\n",s->start->value,val2_new);
                if (val2_new < val2)
                {
                    val2=val2_new;
                    s2=s;
                }
            }
        }
    }


    // val1 en s1 = goedkoopst voorwaarts
    // val2 en s2 = goedkoppst achterwaards

    if (val1 == INT_MAX && val2 == INT_MAX)
    {
        dbg(lvl_error,"no route found, pos blocked\n");
        return NULL;
    }
    if (val1 == val2)
    {
        // wat is het doel hiervan ?

        dbg(lvl_debug,"val1 == val2\n");
        val1=s1->seg_end_out_cost;
        val2=s2->seg_start_out_cost;


    }
    if (val1 < val2)
    {
        dbg(lvl_debug,"val1 < val2\n");
        start=s1->start;
        s=s1;
        dir=1;
    }
    else
    {
        dbg(lvl_debug,"else\n");
        start=s2->end;
        s=s2;
        dir=-1;
    }
    if (pos->street_direction && dir != pos->street_direction && profile->turn_around_penalty)
    {
        if (!route_graph_segment_match(this->avoid_seg,s))
        {
            dbg(lvl_debug,"avoid current segment\n");
            if (this->avoid_seg)
                route_graph_set_traffic_distortion(this, this->avoid_seg, 0);
            this->avoid_seg=s;
            route_graph_set_traffic_distortion(this, this->avoid_seg, profile->turn_around_penalty);
            route_graph_reset(this);
            route_graph_flood_frugal(this, dst, pos, profile, NULL);
            return route_path_new(this, oldpath, pos, dst, profile);
        }
    }
    ret=g_new0(struct route_path, 1);
    ret->in_use=1;
    ret->updated=1;
    if (pos->lenextra) 
        route_path_add_line(ret, &pos->c, &pos->lp, pos->lenextra);
    ret->path_hash=item_hash_new();
    dstinfo=NULL;
    posinfo=pos;
    while (s && !dstinfo)
    { /* following start->seg, which indicates the least costly way to reach our destination */
        segs++;
        if (s->start == start)
        {
            if (item_is_equal(s->data.item, dst->street->item) && (s->end_from_seg == s || !posinfo))
                dstinfo=dst;
            if (!route_path_add_item_from_graph(ret, oldpath, s, 1, posinfo, dstinfo))
                ret->updated=0;
            start=s->end;
            s=s->end_from_seg;

        }
        else
        {
            if (item_is_equal(s->data.item, dst->street->item) && (s->start_from_seg == s || !posinfo))
                dstinfo=dst;
            if (!route_path_add_item_from_graph(ret, oldpath, s, -1, posinfo, dstinfo))
                ret->updated=0;
            start=s->start;
            s=s->start_from_seg;
        }
        posinfo=NULL;
    }
    if (dst->lenextra) 
        route_path_add_line(ret, &dst->lp, &dst->c, dst->lenextra);
    dbg(lvl_debug, "%d segments\n", segs);
    return ret;
}

/**
 * returns false for heightlines map or any other non
 * binfile map or true for maps presumably routable.
 */
static int
is_routable_map(struct map *m)
{
    struct attr map_name_attr;
    if (map_get_attr(m, attr_name, &map_name_attr, NULL)) {
        dbg(lvl_debug, "map name = %s\n", map_name_attr.u.str);
        if(!strstr(map_name_attr.u.str, ".bin")){
            dbg(lvl_error, "not a binfile = %s\n", map_name_attr.u.str);
            return 0;
        }
        if(strstr(map_name_attr.u.str, "heightlines.bin")){
            dbg(lvl_error, "not routable = %s\n", map_name_attr.u.str);
            return 0;
        }
    }
    return 1;
}


static int
route_graph_build_next_map(struct route_graph *rg)
{
    do {
        rg->m=mapset_next(rg->h, 2);
        if (! rg->m) {
            return 0;
        }
        map_rect_destroy(rg->mr);
        rg->mr = NULL;
        if (is_routable_map(rg->m)) {
            rg->mr=map_rect_new(rg->m, rg->sel);
        }
    } while (!rg->mr);
        
    return 1;
}


static int
is_turn_allowed(struct route_graph_point *p, struct route_graph_segment *from, struct route_graph_segment *to)
{
    struct route_graph_point *prev,*next;
    struct route_graph_segment *tmp1,*tmp2;
    if (item_is_equal(from->data.item, to->data.item))
        return 0;
    if (from->start == p)
        prev=from->end;
    else
        prev=from->start;
    if (to->start == p)
        next=to->end;
    else
        next=to->start;
    tmp1=p->end;
    while (tmp1)
    {
        if (tmp1->start->c.x == prev->c.x && tmp1->start->c.y == prev->c.y &&
            (tmp1->data.item.type == type_street_turn_restriction_no ||
            tmp1->data.item.type == type_street_turn_restriction_only))
        {
            tmp2=p->start;
            //dbg(lvl_debug,"found %s (0x%x,0x%x) (0x%x,0x%x)-(0x%x,0x%x) %p-%p\n",item_to_name(tmp1->data.item.type),tmp1->data.item.id_hi,tmp1->data.item.id_lo,tmp1->start->c.x,tmp1->start->c.y,tmp1->end->c.x,tmp1->end->c.y,tmp1->start,tmp1->end);
            while (tmp2)
            {
                //dbg(lvl_debug,"compare %s (0x%x,0x%x) (0x%x,0x%x)-(0x%x,0x%x) %p-%p\n",item_to_name(tmp2->data.item.type),tmp2->data.item.id_hi,tmp2->data.item.id_lo,tmp2->start->c.x,tmp2->start->c.y,tmp2->end->c.x,tmp2->end->c.y,tmp2->start,tmp2->end);
                if (item_is_equal(tmp1->data.item, tmp2->data.item)) 
                    break;
                tmp2=tmp2->start_next;
            }
            //dbg(lvl_debug,"tmp2=%p\n",tmp2);
            //if (tmp2)
            //{
            //  dbg(lvl_debug,"%s tmp2->end=%p next=%p\n",item_to_name(tmp1->data.item.type),tmp2->end,next);
            //}
            if (tmp1->data.item.type == type_street_turn_restriction_no && tmp2 && tmp2->end->c.x == next->c.x && tmp2->end->c.y == next->c.y) {
                //dbg(lvl_debug,"from 0x%x,0x%x over 0x%x,0x%x to 0x%x,0x%x not allowed (no)\n",prev->c.x,prev->c.y,p->c.x,p->c.y,next->c.x,next->c.y);
                return 0;
            }
            if (tmp1->data.item.type == type_street_turn_restriction_only && tmp2 && (tmp2->end->c.x != next->c.x || tmp2->end->c.y != next->c.y)) {
                //dbg(lvl_debug,"from 0x%x,0x%x over 0x%x,0x%x to 0x%x,0x%x not allowed (only)\n",prev->c.x,prev->c.y,p->c.x,p->c.y,next->c.x,next->c.y);
                return 0;
            }
        }
        tmp1=tmp1->end_next;
    }
    //dbg(lvl_debug,"from 0x%x,0x%x over 0x%x,0x%x to 0x%x,0x%x allowed\n",prev->c.x,prev->c.y,p->c.x,p->c.y,next->c.x,next->c.y);
    return 1;
}

static void
route_graph_build_done(struct route_graph *rg, int cancel)
{
    dbg(lvl_debug,"cancel=%d\n",cancel);
//  if (rg->idle_ev)
//      event_remove_idle(rg->idle_ev);
//  if (rg->idle_cb)
//      callback_destroy(rg->idle_cb);
    map_rect_destroy(rg->mr);
        mapset_close(rg->h);
    route_free_selection(rg->sel);
//  rg->idle_ev=NULL;
//  rg->idle_cb=NULL;
    rg->mr=NULL;
    rg->h=NULL;
    rg->sel=NULL;
    if (! cancel) {
//      route_graph_process_restrictions(rg);
        callback_call_0(rg->done_cb);
    }
    rg->busy=0;
}




static void
route_graph_build_idle(struct route_graph *rg, struct vehicleprofile *profile)
{
    struct item *item;
//  double timestamp_build_idle = now_ms();

    //untill done
    while (TRUE)
    {
        for (;;)
        {

            item=map_rect_get_item(rg->mr);
            if (item)
                break;
            if (!route_graph_build_next_map(rg))
            {
//              dbg(0,"build_idle final: %.3f ms\n", now_ms() - timestamp_build_idle);
                route_graph_build_done(rg, 0);
                return;
            }
        }
        if (item->type == type_traffic_distortion)
            route_process_traffic_distortion(rg, item);
        else if (item->type == type_street_turn_restriction_no || item->type == type_street_turn_restriction_only)
            route_graph_add_turn_restriction(rg, item);
        else
            route_graph_add_item(rg, item, profile);
    }
//   dbg(0,"build_idle took: %.3f ms\n",now_ms() - timestamp_build_idle);
}

/**
 * @brief Builds a new route graph from a mapset
 *
 * This function builds a new route graph from a map. Please note that this function does not
 * add any routing information to the route graph - this has to be done via the route_graph_flood()
 * function.
 *
 * The function does not create a graph covering the whole map, but only covering the rectangle
 * between c1 and c2.
 *
 * @param ms The mapset to build the route graph from
 * @param c1 Corner 1 of the rectangle to use from the map
 * @param c2 Corner 2 of the rectangle to use from the map
 * @param done_cb The callback which will be called when graph is complete
 * @return The new route graph.
 */
static struct route_graph *
route_graph_build(struct mapset *ms, struct coord *c, int count, struct callback *done_cb, struct vehicleprofile *profile)
{
    struct route_graph *ret=g_new0(struct route_graph, 1);

    ret->sel=route_calc_selection(c, count, profile);
    ret->h=mapset_open(ms);
    ret->done_cb=done_cb;
    ret->busy=1;
    if  (route_graph_build_next_map(ret))
    {
//      if (async)
//      {
//          ret->idle_cb=callback_new_2(callback_cast(route_graph_build_idle), ret, profile);
//          ret->idle_ev=event_add_idle(50, ret->idle_cb);
//      }
    }
    else
        route_graph_build_done(ret, 0);

    return ret;
}

static void
route_graph_update_done(struct route *this, struct callback *cb)
{
    route_graph_flood_frugal(this->graph, this->current_dst, this->pos, this->vehicleprofile, cb);
}

/**
 * @brief Updates the route graph
 *
 * This updates the route graph after settings in the route have changed. It also
 * adds routing information afterwards by calling route_graph_flood().
 * 
 * @param this The route to update the graph for
 */
static void
route_graph_update(struct route *this, struct callback *cb, int async)
{
    struct attr route_status;
    struct coord *c=g_alloca(sizeof(struct coord)*(1+g_list_length(this->destinations)));
    int i=0;
    GList *tmp;

    route_status.type=attr_route_status;
    route_graph_destroy(this->graph);
    this->graph=NULL;
    callback_destroy(this->route_graph_done_cb);
    this->route_graph_done_cb=callback_new_2(callback_cast(route_graph_update_done), this, cb);
    route_status.u.num=route_status_building_graph;
    route_set_attr(this, &route_status);
    c[i++]=this->pos->c;
    tmp=this->destinations;
    while (tmp)
    {
        struct route_info *dst=tmp->data;
        c[i++]=dst->c;
        tmp=g_list_next(tmp);
    }
    this->graph=route_graph_build(this->ms, c, i, this->route_graph_done_cb, this->vehicleprofile);
//  if (! async)
//  {
//      while (this->graph->busy)
            route_graph_build_idle(this->graph, this->vehicleprofile);
//  }



}

/**
 * @brief Gets street data for an item
 *
 * @param item The item to get the data for
 * @return Street data for the item
 */
struct street_data *
street_get_data (struct item *item)
{
    int count=0,*flags;
    struct street_data *ret = NULL, *ret1;
    struct attr flags_attr, maxspeed_attr;
    const int step = 128;
    int c;

    do
    {
        ret1=g_realloc(ret, sizeof(struct street_data)+(count+step)*sizeof(struct coord));
        if (!ret1)
        {
            if (ret)
                g_free(ret);
            return NULL;
        }
        ret = ret1;
        c = item_coord_get(item, &ret->c[count], step);
        count += c;
    }
    while (c && c == step);

    ret1=g_realloc(ret, sizeof(struct street_data)+count*sizeof(struct coord));
    if (ret1)
        ret = ret1;
    ret->item=*item;
    ret->count=count;
    if (item_attr_get(item, attr_flags, &flags_attr)) 
        ret->flags=flags_attr.u.num;
    else
    {
        flags=item_get_default_flags(item->type);
        if (flags)
            ret->flags=*flags;
        else
            ret->flags=0;
    }

    ret->maxspeed = -1;
    if (ret->flags & AF_SPEED_LIMIT)
    {
        if (item_attr_get(item, attr_maxspeed, &maxspeed_attr))
        {
            ret->maxspeed = maxspeed_attr.u.num;
        }
    }

    return ret;
}

/**
 * @brief Copies street data
 * 
 * @param orig The street data to copy
 * @return The copied street data
 */ 
struct street_data *
street_data_dup(struct street_data *orig)
{
    struct street_data *ret;
    int size=sizeof(struct street_data)+orig->count*sizeof(struct coord);

    ret=g_malloc(size);
    memcpy(ret, orig, size);

    return ret;
}

/**
 * @brief Frees street data
 *
 * @param sd Street data to be freed
 */
void
street_data_free(struct street_data *sd)
{
    g_free(sd);
}

/**
 * @brief Finds the nearest street to a given coordinate
 *
 * @param ms The mapset to search in for the street
 * @param pc The coordinate to find a street nearby
 * @return The nearest street
 */
static struct route_info *
route_find_nearest_street(struct vehicleprofile *vehicleprofile, struct mapset *ms, struct pcoord *pc)
{
    struct route_info *ret=NULL;
    int max_dist=1000;
    struct map_selection *sel;
    int dist,mindist=0,pos;
    struct mapset_handle *h;
    struct map *m;
    struct map_rect *mr;
    struct item *item;
    struct coord lp;
    struct street_data *sd;
    struct coord c;
    struct coord_geo g;
    
    if(!vehicleprofile)
        return NULL;

    ret=g_new0(struct route_info, 1);
    mindist = INT_MAX;

    h=mapset_open(ms);
    while ((m=mapset_next(h,2))) {
        c.x = pc->x;
        c.y = pc->y;
        if (map_projection(m) != pc->pro) {
            transform_to_geo(pc->pro, &c, &g);
            transform_from_geo(map_projection(m), &g, &c);
        }
        sel = route_rect(18, &c, &c, 0, max_dist);
        if (!sel)
            continue;
        mr=map_rect_new(m, sel);
        if (!mr) {
            map_selection_destroy(sel);
            continue;
        }
        while ((item=map_rect_get_item(mr))) {
            if (item_get_default_flags(item->type)) {
                sd=street_get_data(item);
                if (!sd)
                    continue;
                dist=transform_distance_polyline_sq(sd->c, sd->count, &c, &lp, &pos);
                if (dist < mindist && (
                    (sd->flags & vehicleprofile->flags_forward_mask) == vehicleprofile->flags ||
                    (sd->flags & vehicleprofile->flags_reverse_mask) == vehicleprofile->flags)) {
                    mindist = dist;
                    if (ret->street) {
                        street_data_free(ret->street);
                    }
                    ret->c=c;
                    ret->lp=lp;
                    ret->pos=pos;
                    ret->street=sd;
                    dbg(lvl_debug,"dist=%d id 0x%x 0x%x pos=%d\n", dist, item->id_hi, item->id_lo, pos);
                } else {
                    street_data_free(sd);
                }
            }
        }
        map_selection_destroy(sel);
        map_rect_destroy(mr);
    }
    mapset_close(h);

    if (!ret->street || mindist > max_dist*max_dist) {
        if (ret->street) {
            street_data_free(ret->street);
            dbg(lvl_debug,"Much too far %d > %d\n", mindist, max_dist);
        }
        g_free(ret);
        ret = NULL;
    }

    return ret;
}

/**
 * @brief Destroys a route_info
 *
 * @param info The route info to be destroyed
 */
void
route_info_free(struct route_info *inf)
{
    if (!inf)
        return;
    if (inf->street)
        street_data_free(inf->street);
    g_free(inf);
}

/**
 * @brief Returns street data for a route info 
 *
 * @param rinf The route info to return the street data for
 * @return Street data for the route info
 */
struct street_data *
route_info_street(struct route_info *rinf)
{
    return rinf->street;
}

#if 0
struct route_crossings *
route_crossings_get(struct route *this, struct coord *c)
{
      struct route_point *pnt;
      struct route_segment *seg;
      int crossings=0;
      struct route_crossings *ret;

       pnt=route_graph_get_point(this, c);
       seg=pnt->start;
       while (seg) {
        printf("start: 0x%x 0x%x\n", seg->item.id_hi, seg->item.id_lo);
               crossings++;
               seg=seg->start_next;
       }
       seg=pnt->end;
       while (seg) {
        printf("end: 0x%x 0x%x\n", seg->item.id_hi, seg->item.id_lo);
               crossings++;
               seg=seg->end_next;
       }
       ret=g_malloc(sizeof(struct route_crossings)+crossings*sizeof(struct route_crossing));
       ret->count=crossings;
       return ret;
}
#endif


struct map_rect_priv {
    struct route_info_handle *ri;
    enum attr_type attr_next;
    int pos;
    struct map_priv *mpriv;
    struct item item;
    unsigned int last_coord;
    struct route_path *path;
    struct route_path_segment *seg,*seg_next;
    struct route_graph_point *point;
    struct route_graph_segment *rseg;
    char *str;
    int hash_bucket;
    struct coord *coord_sel;    /**< Set this to a coordinate if you want to filter for just a single route graph point */
    struct route_graph_point_iterator it;
    /* Pointer to current waypoint element of route->destinations */
    GList *dest;
};

static void
rm_coord_rewind(void *priv_data)
{
    struct map_rect_priv *mr = priv_data;
    mr->last_coord = 0;
}

static void
rm_attr_rewind(void *priv_data)
{
    struct map_rect_priv *mr = priv_data;
    mr->attr_next = attr_street_item;
}

static int
rm_attr_get(void *priv_data, enum attr_type attr_type, struct attr *attr)
{
    struct map_rect_priv *mr = priv_data;
    struct route_path_segment *seg=mr->seg;
    struct route *route=mr->mpriv->route;
    if (mr->item.type != type_street_route && mr->item.type != type_waypoint && mr->item.type != type_route_end)
        return 0;
    attr->type=attr_type;
    switch (attr_type) {
        case attr_any:
            while (mr->attr_next != attr_none)
            {
                if (rm_attr_get(priv_data, mr->attr_next, attr))
                    return 1;
            }
            return 0;
        case attr_maxspeed:
            mr->attr_next = attr_street_item;
            if (seg && seg->data->flags & AF_SPEED_LIMIT)
            {
                attr->u.num=RSD_MAXSPEED(seg->data);
            }
            else
            {
                return 0;
            }
            return 1;
        case attr_street_item:
            mr->attr_next=attr_direction;
            if (seg && seg->data->item.map)
                attr->u.item=&seg->data->item;
            else
                return 0;
            return 1;
        case attr_direction:
            mr->attr_next=attr_route;
            if (seg) 
                attr->u.num=seg->direction;
            else
                return 0;
            return 1;
        case attr_route:
            mr->attr_next=attr_length;
            attr->u.route = mr->mpriv->route;
            return 1;
        case attr_length:
            mr->attr_next=attr_time;
            if (seg)
                attr->u.num=seg->data->len;
            else
                return 0;
            return 1;
        case attr_time:
            mr->attr_next=attr_speed;
            if (seg)
                attr->u.num=route_time_seg(route->vehicleprofile, seg->data, NULL);
            else
                return 0;
            return 1;
        case attr_speed:
            mr->attr_next=attr_label;
            if (seg)
                attr->u.num=route_seg_speed(route->vehicleprofile, seg->data, NULL);
            else
                return 0;
            return 1;
        case attr_label:
            mr->attr_next=attr_none;
            if(mr->item.type==type_waypoint || mr->item.type == type_route_end)
            {
                if(mr->str)
                    g_free(mr->str);
                mr->str=g_strdup_printf("%d",route->reached_destinations_count+g_list_position(route->destinations,mr->dest)+1);
                attr->u.str=mr->str;
                return 1;
            }
            return 0;
        default:
            mr->attr_next=attr_none;
            attr->type=attr_none;
            return 0;
    }
    return 0;
}

static int
rm_coord_get(void *priv_data, struct coord *c, int count)
{
    struct map_rect_priv *mr = priv_data;
    struct route_path_segment *seg = mr->seg;
    int i,rc=0;
    struct route *r = mr->mpriv->route;
    enum projection pro = route_projection(r);

    if (pro == projection_none)
        return 0;
    if (mr->item.type == type_route_start || mr->item.type == type_route_start_reverse || mr->item.type == type_route_end || mr->item.type == type_waypoint )
    {
        if (! count || mr->last_coord)
            return 0;
        mr->last_coord=1;
        if (mr->item.type == type_route_start || mr->item.type == type_route_start_reverse)
            c[0]=r->pos->c;
        else if (mr->item.type == type_waypoint)
        {
            c[0]=((struct route_info *)mr->dest->data)->c;
        }
        else
        { /*type_route_end*/
            c[0]=route_get_dst(r)->c;
        }
        return 1;
    }
    if (! seg)
        return 0;
    for (i=0; i < count; i++)
    {
        if (mr->last_coord >= seg->ncoords)
            break;
        if (i >= seg->ncoords)
            break;
        if (pro != projection_mg)
            transform_from_to(&seg->c[mr->last_coord++], pro,
                &c[i],projection_mg);
        else
            c[i] = seg->c[mr->last_coord++];
        rc++;
    }
    dbg(lvl_debug,"return %d\n",rc);
    return rc;
}

static struct item_methods methods_route_item = {
    rm_coord_rewind,
    rm_coord_get,
    rm_attr_rewind,
    rm_attr_get,
};

static void
rp_attr_rewind(void *priv_data)
{
    struct map_rect_priv *mr = priv_data;
    mr->attr_next = attr_label;
}

static int
rp_attr_get(void *priv_data, enum attr_type attr_type, struct attr *attr)
{
    struct map_rect_priv *mr = priv_data;
    struct route_graph_point *p = mr->point;
    struct route_graph_segment *seg = mr->rseg;
    struct route *route=mr->mpriv->route;

    attr->type=attr_type;
    switch (attr_type) {
    case attr_any: // works only with rg_points for now
        while (mr->attr_next != attr_none)
        {
            dbg(lvl_debug,"querying %s\n", attr_to_name(mr->attr_next));
            if (rp_attr_get(priv_data, mr->attr_next, attr))
                return 1;
        }
        return 0;
    case attr_maxspeed:
        mr->attr_next = attr_label;
        if (mr->item.type != type_rg_segment) 
            return 0;
        if (seg && (seg->data.flags & AF_SPEED_LIMIT))
        {
            attr->type = attr_maxspeed;
            attr->u.num=RSD_MAXSPEED(&seg->data);
            return 1;
        }
        else
        {
            return 0;
        }
    case attr_label:
        mr->attr_next=attr_street_item;
        attr->type = attr_label;
        if (mr->str)
            g_free(mr->str);
        if (mr->item.type == type_rg_point)
        {
            int lowest_cost = INT_MAX;

            // iets betekenisvol van maken

            if (p->start && p->start->seg_start_out_cost  < lowest_cost)
            {
                lowest_cost = p->start->seg_start_out_cost;
            }
            if (p->end && p->end->seg_end_out_cost < lowest_cost)
            {
                lowest_cost = p->end->seg_end_out_cost;
            }

            if (lowest_cost < INT_MAX)
                mr->str=g_strdup_printf("%d", lowest_cost);

            else
                mr->str=g_strdup("-");
        }
        else
        {
            int len=seg->data.len;
            int speed=route_seg_speed(route->vehicleprofile, &seg->data, NULL);
            int time=route_time_seg(route->vehicleprofile, &seg->data, NULL);
            if (speed)
                mr->str=g_strdup_printf("%dm %dkm/h %d.%ds",len,speed,time/10,time%10);
            else if (len)
                mr->str=g_strdup_printf("%dm",len);
            else
            {
                mr->str=NULL;
                return 0;
            }
        }
        attr->u.str = mr->str;
        return 1;
    case attr_street_item:
        mr->attr_next=attr_flags;
        if (mr->item.type != type_rg_segment) 
            return 0;
        if (seg && seg->data.item.map)
            attr->u.item=&seg->data.item;
        else
            return 0;
        return 1;
    case attr_flags:
        mr->attr_next = attr_direction;
        if (mr->item.type != type_rg_segment)
            return 0;
        if (seg)
        {
            attr->u.num = seg->data.flags;
        }
        else
        {
            return 0;
        }
        return 1;
    case attr_direction:
        mr->attr_next = attr_debug;
        // This only works if the map has been opened at a single point, and in that case indicates if the
        // segment returned last is connected to this point via its start (1) or its end (-1)
        if (!mr->coord_sel || (mr->item.type != type_rg_segment))
            return 0;
        if (seg->start == mr->point)
        {
            attr->u.num=1;
        }
        else if (seg->end == mr->point)
        {
            attr->u.num=-1;
        }
        else
        {
            return 0;
        }
        return 1;
    case attr_debug:
        mr->attr_next=attr_none;
        if (mr->str)
            g_free(mr->str);
        mr->str=NULL;
        switch (mr->item.type)
        {
            case type_rg_point:
            {
                struct route_graph_segment *tmp;
                int start=0;
                int end=0;
                tmp=p->start;
                while (tmp)
                {
                    start++;
                    tmp=tmp->start_next;
                }
                tmp=p->end;
                while (tmp)
                {
                    end++;
                    tmp=tmp->end_next;
                }
                mr->str=g_strdup_printf("%d %d %p (0x%x,0x%x)", start, end, p, p->c.x, p->c.y);
                attr->u.str = mr->str;
            }
                return 1;
            case type_rg_segment:
                if (! seg)
                    return 0;
                mr->str=g_strdup_printf("len %d time %d start %p end %p",seg->data.len, route_time_seg(route->vehicleprofile, &seg->data, NULL), seg->start, seg->end);
                attr->u.str = mr->str;
                return 1;
            default:
                return 0;
        }
    default:
        mr->attr_next=attr_none;
        attr->type=attr_none;
        return 0;
    }
}



/**
 * @brief Returns the coordinates of a route graph item
 *
 * @param priv_data The route graph item's private data
 * @param c Pointer where to store the coordinates
 * @param count How many coordinates to get at a max?
 * @return The number of coordinates retrieved
 */
static int
rp_coord_get(void *priv_data, struct coord *c, int count)
{
    struct map_rect_priv *mr = priv_data;
    struct route_graph_point *p = mr->point;
    struct route_graph_segment *seg = mr->rseg;
    int rc = 0,i,dir;
    struct route *r = mr->mpriv->route;
    enum projection pro = route_projection(r);

    if (pro == projection_none)
        return 0;
    for (i=0; i < count; i++)
    {
        if (mr->item.type == type_rg_point)
        {
            if (mr->last_coord >= 1)
                break;
            if (pro != projection_mg)
                transform_from_to(&p->c, pro,
                    &c[i],projection_mg);
            else
                c[i] = p->c;
        }
        else
        {
            if (mr->last_coord >= 2)
                break;
            dir=0;

//          if (seg->end->seg == seg)
                dir=1;
            if (mr->last_coord)
                dir=1-dir;
            if (dir)
            {
                if (pro != projection_mg)
                    transform_from_to(&seg->end->c, pro,
                        &c[i],projection_mg);
                else
                    c[i] = seg->end->c;
            }
            else
            {
                if (pro != projection_mg)
                    transform_from_to(&seg->start->c, pro,
                        &c[i],projection_mg);
                else
                    c[i] = seg->start->c;
            }
        }
        mr->last_coord++;
        rc++;
    }
    return rc;
}


static struct item_methods methods_point_item = {
    rm_coord_rewind,
    rp_coord_get,
    rp_attr_rewind,
    rp_attr_get,
};

static void
rp_destroy(struct map_priv *priv)
{
    g_free(priv);
}

static void
rm_destroy(struct map_priv *priv)
{
    g_free(priv);
}

static struct map_rect_priv * 
rm_rect_new(struct map_priv *priv, struct map_selection *sel)
{
    struct map_rect_priv * mr;
    dbg(lvl_debug,"enter\n");
#if 0
    if (! route_get_pos(priv->route))
        return NULL;
    if (! route_get_dst(priv->route))
        return NULL;
#endif
#if 0
    if (! priv->route->path2)
        return NULL;
#endif
    mr=g_new0(struct map_rect_priv, 1);
    mr->mpriv = priv;
    mr->item.priv_data = mr;
    mr->item.type = type_none;
    mr->item.meth = &methods_route_item;
    if (priv->route->path2)
    {
        mr->path=priv->route->path2;
        mr->seg_next=mr->path->path;
        mr->path->in_use++;
    }
    else
        mr->seg_next=NULL;
    return mr;
}

/**
 * @brief Opens a new map rectangle on the route graph's map
 *
 * This function opens a new map rectangle on the route graph's map.
 * The "sel" parameter enables you to only search for a single route graph
 * point on this map (or exactly: open a map rectangle that only contains
 * this one point). To do this, pass here a single map selection, whose 
 * c_rect has both coordinates set to the same point. Otherwise this parameter
 * has no effect.
 *
 * @param priv The route graph map's private data
 * @param sel Here it's possible to specify a point for which to search. Please read the function's description.
 * @return A new map rect's private data
 */
static struct map_rect_priv * 
rp_rect_new(struct map_priv *priv, struct map_selection *sel)
{
    struct map_rect_priv * mr;

    dbg(lvl_debug,"enter\n");
    if (! priv->route->graph)
        return NULL;
    mr=g_new0(struct map_rect_priv, 1);
    mr->mpriv = priv;
    mr->item.priv_data = mr;
    mr->item.type = type_rg_point;
    mr->item.meth = &methods_point_item;
    if (sel)
    {
        if ((sel->u.c_rect.lu.x == sel->u.c_rect.rl.x) && (sel->u.c_rect.lu.y == sel->u.c_rect.rl.y))
        {
            mr->coord_sel = g_malloc(sizeof(struct coord));
            *(mr->coord_sel) = sel->u.c_rect.lu;
        }
    }
    return mr;
}

static void
rm_rect_destroy(struct map_rect_priv *mr)
{
    if (mr->str)
        g_free(mr->str);
    if (mr->coord_sel)
    {
        g_free(mr->coord_sel);
    }
    if (mr->path)
    {
        mr->path->in_use--;
        if (mr->path->update_required && (mr->path->in_use==1)) 
            route_path_update_done(mr->mpriv->route, mr->path->update_required-1);
        else if (!mr->path->in_use)
            g_free(mr->path);
    }
    g_free(mr);
}

static struct item *
rp_get_item(struct map_rect_priv *mr)
{
    struct route *r = mr->mpriv->route;
    struct route_graph_point *p = mr->point;
    struct route_graph_segment *seg = mr->rseg;



    if (mr->item.type == type_rg_point) {

        dbg(lvl_debug,"rp_get_item type_rg_point\n");

        if (mr->coord_sel) {
            // We are supposed to return only the point at one specified coordinate...
            if (!p) {
                p = route_graph_get_point_last(r->graph, mr->coord_sel);
                if (!p) {
                    mr->point = NULL; // This indicates that no point has been found
                } else {
                    mr->it = rp_iterator_new(p);
                }
            } else {
                p = NULL;
            }
        } else {
            if (!p) {
                mr->hash_bucket=0;
                p = r->graph->hash[0];
            } else 
                p=p->hash_next;
            while (!p) {
                mr->hash_bucket++;
                if (mr->hash_bucket >= HASH_SIZE)
                    break;
                p = r->graph->hash[mr->hash_bucket];
            }
        }
        if (p) {
            mr->point = p;
            mr->item.id_lo++;
            rm_coord_rewind(mr);
            rp_attr_rewind(mr);
            return &mr->item;
        } else 
            mr->item.type = type_rg_segment;
    }

    if (mr->coord_sel) {
        if (!mr->point) { /* This means that no point has been found */
            return NULL;
        }
        seg = rp_iterator_next(&(mr->it));
    } else {
        if (!seg)
            seg=r->graph->route_segments;
        else
            seg=seg->next;
    }
    
    mr->item.id_hi = 0;

    if (seg) {

        mr->rseg = seg;
        dbg(lvl_debug,"rp_get_item id_lo = %i, len=%i type= %s\n",mr->item.id_lo,(seg->data.len
                ? seg->data.len : 0),item_to_name(mr->item.type));
        mr->item.id_lo++;
        rm_coord_rewind(mr);
        rp_attr_rewind(mr);

        return &mr->item;
    }
    return NULL;
}

static struct item *
rp_get_item_byid(struct map_rect_priv *mr, int id_hi, int id_lo)
{
    struct item *ret=NULL;
    while (id_lo-- > 0) 
        ret=rp_get_item(mr);
    return ret;
}


static struct item *
rm_get_item(struct map_rect_priv *mr)
{
    struct route *route=mr->mpriv->route;
    void *id=0;

    switch (mr->item.type) {
    case type_none:
        if (route->pos && route->pos->street_direction && route->pos->street_direction != route->pos->dir)
            mr->item.type=type_route_start_reverse;
        else
            mr->item.type=type_route_start;
        if (route->pos) {
            id=route->pos;
            break;
        }

    case type_route_start:
    case type_route_start_reverse:
        mr->seg=NULL;
        mr->dest=mr->mpriv->route->destinations;
    default:
        if (mr->item.type == type_waypoint)
            mr->dest=g_list_next(mr->dest);
        mr->item.type=type_street_route;
        mr->seg=mr->seg_next;
        if (!mr->seg && mr->path && mr->path->next) {
            struct route_path *p=NULL;
            mr->path->in_use--;
            if (!mr->path->in_use)
                p=mr->path;
            mr->path=mr->path->next;
            mr->path->in_use++;
            mr->seg=mr->path->path;
            if (p)
                g_free(p);
            if (mr->dest) {
                id=mr->dest;
                mr->item.type=type_waypoint;
                mr->seg_next=mr->seg;
                break;
            }
        }
        if (mr->seg) {
            mr->seg_next=mr->seg->next;
            id=mr->seg;
            break;
        }
        if (mr->dest && g_list_next(mr->dest)) {
            id=mr->dest;
            mr->item.type=type_waypoint;
            break;
        }
        mr->item.type=type_route_end;
        id=&(mr->mpriv->route->destinations);
        if (mr->mpriv->route->destinations)
            break;
    case type_route_end:
        return NULL;
    }
    mr->last_coord = 0;
    item_id_from_ptr(&mr->item,id);
    rm_attr_rewind(mr);
    return &mr->item;
}

static struct item *
rm_get_item_byid(struct map_rect_priv *mr, int id_hi, int id_lo)
{
    struct item *ret=NULL;
    do {
        ret=rm_get_item(mr);
    } while (ret && (ret->id_lo!=id_lo || ret->id_hi!=id_hi));
    return ret;
}

static struct map_methods route_meth = {
    projection_mg,
    "utf-8",
    rm_destroy,
    rm_rect_new,
    rm_rect_destroy,
    rm_get_item,
    rm_get_item_byid,
    NULL,
    NULL,
    NULL,
};

static struct map_methods route_graph_meth = {
    projection_mg,
    "utf-8",
    rp_destroy,
    rp_rect_new,
    rm_rect_destroy,
    rp_get_item,
    rp_get_item_byid,
    NULL,
    NULL,
    NULL,
};

static struct map_priv *
route_map_new_helper(struct map_methods *meth, struct attr **attrs, int graph)
{
    struct map_priv *ret;
    struct attr *route_attr;

    route_attr=attr_search(attrs, NULL, attr_route);
    if (! route_attr)
        return NULL;
    ret=g_new0(struct map_priv, 1);
    if (graph)
        *meth=route_graph_meth;
    else
        *meth=route_meth;
    ret->route=route_attr->u.route;

    return ret;
}

static struct map_priv *
route_map_new(struct map_methods *meth, struct attr **attrs, struct callback_list *cbl)
{
    return route_map_new_helper(meth, attrs, 0);
}

static struct map_priv *
route_graph_map_new(struct map_methods *meth, struct attr **attrs, struct callback_list *cbl)
{
    return route_map_new_helper(meth, attrs, 1);
}

static struct map *
route_get_map_helper(struct route *this_, struct map **map, char *type, char *description)
{
    struct attr *attrs[5];
    struct attr a_type,navigation,data,a_description;
    a_type.type=attr_type;
    a_type.u.str=type;
    navigation.type=attr_route;
    navigation.u.route=this_;
    data.type=attr_data;
    data.u.str="";
    a_description.type=attr_description;
    a_description.u.str=description;

    attrs[0]=&a_type;
    attrs[1]=&navigation;
    attrs[2]=&data;
    attrs[3]=&a_description;
    attrs[4]=NULL;

    if (! *map)
    {
        *map=map_new(NULL, attrs);
        navit_object_ref((struct navit_object *)*map);
    }
 
    return *map;
}

/**
 * @brief Returns a new map containing the route path
 *
 * This function returns a new map containing the route path.
 *
 * @important Do not map_destroy() this!
 *
 * @param this_ The route to get the map of
 * @return A new map containing the route path
 */
struct map *
route_get_map(struct route *this_)
{
    return route_get_map_helper(this_, &this_->map, "route","Route");
}


/**
 * @brief Returns a new map containing the route graph
 *
 * This function returns a new map containing the route graph.
 *
 * @important Do not map_destroy()  this!
 *
 * @param this_ The route to get the map of
 * @return A new map containing the route graph
 */
struct map *
route_get_graph_map(struct route *this_)
{
    return route_get_map_helper(this_, &this_->graph_map, "route_graph","Route Graph");
}

void
route_set_projection(struct route *this_, enum projection pro)
{
}

int
route_set_attr(struct route *this_, struct attr *attr)
{
    int attr_updated=0;
    switch (attr->type) {
    case attr_route_status:
        attr_updated = (this_->route_status != attr->u.num);
        this_->route_status = attr->u.num;
        break;
    case attr_destination:
        route_set_destination(this_, attr->u.pcoord, 1);
        return 1;
    case attr_position:
        route_set_position_flags(this_, attr->u.pcoord, route_path_flag_async);
        return 1;
    case attr_position_test:
        return route_set_position_flags(this_, attr->u.pcoord, route_path_flag_no_rebuild);
    case attr_vehicle:
        attr_updated = (this_->v != attr->u.vehicle);
        this_->v=attr->u.vehicle;
        if (attr_updated) {
            struct attr g;
            struct pcoord pc;
            struct coord c;
            if (vehicle_get_attr(this_->v, attr_position_coord_geo, &g, NULL)) {
                pc.pro=projection_mg;
                transform_from_geo(projection_mg, g.u.coord_geo, &c);
                pc.x=c.x;
                pc.y=c.y;
                route_set_position(this_, &pc);
            }
        }
        break;
    default:
        dbg(lvl_error,"unsupported attribute: %s\n",attr_to_name(attr->type));
        return 0;
    }
    if (attr_updated)
        callback_list_call_attr_2(this_->cbl2, attr->type, this_, attr);
    return 1;
}

int
route_add_attr(struct route *this_, struct attr *attr)
{
    switch (attr->type) {
    case attr_callback:
        callback_list_add(this_->cbl2, attr->u.callback);
        return 1;
    default:
        return 0;
    }
}

int
route_remove_attr(struct route *this_, struct attr *attr)
{
    dbg(lvl_debug,"enter\n");
    switch (attr->type) {
    case attr_callback:
        callback_list_remove(this_->cbl2, attr->u.callback);
        return 1;
    case attr_vehicle:
        this_->v=NULL;
        return 1;
    default:
        return 0;
    }
}

int
route_get_attr(struct route *this_, enum attr_type type, struct attr *attr, struct attr_iter *iter)
{
    int ret=1;
    switch (type) {
    case attr_map:
        attr->u.map=route_get_map(this_);
        ret=(attr->u.map != NULL);
        break;
    case attr_destination:
        if (this_->destinations) {
            struct route_info *dst;
            if (iter) {
                if (iter->u.list) {
                    iter->u.list=g_list_next(iter->u.list);
                } else {
                    iter->u.list=this_->destinations;
                }
                if (!iter->u.list) {
                    return 0;
                }
                dst = (struct route_info*)iter->u.list->data;               
            } else { //No iter handling
                dst=route_get_dst(this_);
            }
            attr->u.pcoord=&this_->pc;
            this_->pc.pro=projection_mg; /* fixme */
            this_->pc.x=dst->c.x;
            this_->pc.y=dst->c.y;
        } else
            ret=0;
        break;
    case attr_vehicle:
        attr->u.vehicle=this_->v;
        ret=(this_->v != NULL);
        dbg(lvl_debug,"get vehicle %p\n",this_->v);
        break;
    case attr_vehicleprofile:
        attr->u.vehicleprofile=this_->vehicleprofile;
        ret=(this_->vehicleprofile != NULL);
        break;
    case attr_route_status:
        attr->u.num=this_->route_status;
        break;
    case attr_destination_time:
        if (this_->path2 && (this_->route_status == route_status_path_done_new || this_->route_status == route_status_path_done_incremental)) {
            struct route_path *path=this_->path2;
            attr->u.num=0;
            while (path) {
                attr->u.num+=path->path_time;
                path=path->next;
            }
            dbg(lvl_debug,"path_time %ld\n",attr->u.num);
        } else
            ret=0;
        break;
    case attr_destination_length:
        if (this_->path2 && (this_->route_status == route_status_path_done_new || this_->route_status == route_status_path_done_incremental)) {
            struct route_path *path=this_->path2;
            attr->u.num=0;
            while (path) {
                attr->u.num+=path->path_len;
                path=path->next;
            }
        } else
            ret=0;
        break;
    default:
        return 0;
    }
    attr->type=type;
    return ret;
}

struct attr_iter *
route_attr_iter_new(void)
{
    return g_new0(struct attr_iter, 1);
}

void
route_attr_iter_destroy(struct attr_iter *iter)
{
    g_free(iter);
}

void
route_init(void)
{
    plugin_register_map_type("route", route_map_new);
    plugin_register_map_type("route_graph", route_graph_map_new);
}

void
route_destroy(struct route *this_)
{
    this_->refcount++; /* avoid recursion */
    route_path_destroy(this_->path2,1);
    route_graph_destroy(this_->graph);
    route_clear_destinations(this_);
    route_info_free(this_->pos);
    map_destroy(this_->map);
    map_destroy(this_->graph_map);
    g_free(this_);
}

struct object_func route_func = {
        attr_route,
        (object_func_new)route_new,
        (object_func_get_attr)route_get_attr,
        (object_func_iter_new)NULL,
        (object_func_iter_destroy)NULL,
        (object_func_set_attr)route_set_attr,
        (object_func_add_attr)route_add_attr,
        (object_func_remove_attr)route_remove_attr,
        (object_func_init)NULL,
        (object_func_destroy)route_destroy,
        (object_func_dup)route_dup,
        (object_func_ref)navit_object_ref,
        (object_func_unref)navit_object_unref,
};
