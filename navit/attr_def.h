/*
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2014 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/** @file attr_def.h
 * @brief Attribute definitions.
 * 
 * Any attribute used by a Navit object must be defined in this file.
 *
 * @author Navit Team
 * @date 2005-2014
 */

/* prototypes */

/* common */
ATTR2(0x00000000,none)
ATTR(any)
ATTR(any_xml)

ATTR2(0x00010000,type_item_begin)
ATTR(town_streets_item)
ATTR_UNUSED
ATTR_UNUSED
ATTR(street_item)
ATTR_UNUSED
ATTR(position_sat_item)
ATTR(current_item)
ATTR2(0x0001ffff,type_item_end)

ATTR2(0x00020000,type_int_begin)
ATTR_UNUSED
ATTR(id)
ATTR(flags)
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR(flush_size)
ATTR(flush_time)
ATTR(zipfile_ref)
ATTR(country_id)
ATTR(position_sats)
ATTR(position_sats_used)
ATTR(update)
ATTR(follow)
ATTR(length)
ATTR(time)
ATTR(destination_length)
ATTR(destination_time)
ATTR(speed)
ATTR(interval)
ATTR(position_qual)
ATTR(zoom)
ATTR(retry_interval)
ATTR(projection)
ATTR(offroad)
ATTR(vocabulary_name)
ATTR(vocabulary_name_systematic)
ATTR(vocabulary_distances)
ATTR_UNUSED
ATTR(antialias)
ATTR(order_delta)
ATTR(baudrate)
ATTR_UNUSED
ATTR(icon_xs)
ATTR(icon_l)
ATTR(icon_s)
ATTR(spacing)
ATTR(recent_dest)
ATTR(destination_distance)
ATTR(check_version)
ATTR(details)
ATTR(width)
ATTR(offset)
ATTR(directed)
ATTR(radius)
ATTR(text_size)
ATTR(level)
ATTR(icon_w)
ATTR(icon_h)
ATTR(rotation)
ATTR(checksum_ignore)
ATTR(position_fix_type)
ATTR(timeout)
ATTR(orientation)
ATTR(keyboard)
ATTR(position_sats_signal)
ATTR(cps)
ATTR_UNUSED
ATTR(osd_configuration)
ATTR(columns)
ATTR(align)
ATTR(sat_prn)
ATTR(sat_elevation)
ATTR(sat_azimuth)
ATTR(sat_snr)
ATTR(autozoom)
ATTR(version)
ATTR(autozoom_min)
ATTR(maxspeed)
ATTR(cdf_histsize)
ATTR(message_maxage)
ATTR(message_maxnum)
ATTR(pitch)
ATTR_UNUSED
ATTR_UNUSED
ATTR(route_status)
ATTR(route_weight)
ATTR(roundabout_weight)
ATTR(route_mode)
ATTR(maxspeed_handling)
ATTR(flags_forward_mask)
ATTR(flags_reverse_mask)
ATTR(link_weight)
ATTR(delay)
ATTR(lag)
ATTR(bpp)
ATTR(fullscreen)
ATTR(windowid)
ATTR(hog)
ATTR(flags_town)
ATTR(flags_street)
ATTR(flags_house_number)
ATTR(use_camera)
ATTR(flags_graphics)
ATTR(zoom_min)
ATTR(zoom_max)
ATTR(gamma)
ATTR(brightness)
ATTR(contrast)
ATTR(height)
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR(shmkey)
ATTR(vehicle_width)
ATTR(vehicle_length)
ATTR(vehicle_height)
ATTR(vehicle_weight)
ATTR(vehicle_axle_weight)
ATTR(vehicle_dangerous_goods)
ATTR(shmsize)
ATTR(shmoffset)
ATTR_UNUSED
ATTR(static_speed)
ATTR(static_distance)
ATTR(through_traffic_penalty)
ATTR(through_traffic_flags)
ATTR(speed_exceed_limit_offset)
ATTR(speed_exceed_limit_percent)
ATTR(map_border)
ATTR(angle_pref)
ATTR(connected_pref)
ATTR(nostop_pref)
ATTR(offroad_limit_pref)
ATTR(route_pref)
ATTR(overspeed_pref)
ATTR(overspeed_percent_pref)
ATTR(autosave_period)
ATTR(tec_type)
ATTR(tec_dirtype)
ATTR(tec_direction)
ATTR(imperial)
ATTR(update_period)
ATTR(tunnel_extrapolation)
ATTR(street_count)
ATTR(min_dist)
ATTR(max_dist)
ATTR(cache_size)
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR(turn_around_count)
ATTR(turn_around_penalty)
ATTR(turn_around_penalty2)
ATTR(autozoom_max)
ATTR2(0x00027500,type_rel_abs_begin)
/* These attributes are int that can either hold relative		*
 * or absolute values. A relative value is indicated by 		*
 * adding 0x60000000.							*
 *									*
 * The range of valid absolute values is -0x40000000 to			*
 * 0x40000000, the range of relative values is from			*
 * -0x20000000 to 0x20000000.						*/
ATTR(h)
ATTR(w)
ATTR(x)
ATTR(y)
ATTR(font_size)

ATTR2(0x00028000,type_boolean_begin)
/* boolean */
ATTR(overwrite)
ATTR(active)
ATTR(follow_cursor)
ATTR_UNUSED
ATTR(tracking)
ATTR(menubar)
ATTR(statusbar)
ATTR(toolbar)
ATTR(animate)
ATTR(lazy)
ATTR(mkdir)
ATTR(predraw)
ATTR(postdraw)
ATTR(button)
ATTR(ondemand)
ATTR(menu_on_map_click)
ATTR(direction)
ATTR_UNUSED
ATTR(gui_speech)
ATTR(town_id) /* fixme? */
ATTR(street_id) /* fixme? */
ATTR(district_id) /* fixme? */
ATTR(drag_bitmap)
ATTR(use_mousewheel)
ATTR_UNUSED
ATTR(position_magnetic_direction)
ATTR(use_overlay)
ATTR_UNUSED
ATTR(autozoom_active)
ATTR(position_valid)
ATTR(frame)
ATTR(tell_street_name)
ATTR(bluetooth)
ATTR(signal_on_map_click)
ATTR(route_active)
ATTR(search_active)
ATTR(unsuspend)
ATTR(announce_on)
ATTR(disable_reset)
ATTR(autostart)
ATTR(readwrite)
ATTR(cache)
ATTR(create)
ATTR(persistent)
ATTR(waypoints_flag) /* toggle for "set as destination" to switch between start a new route or add */
ATTR(no_warning_if_map_file_missing)
ATTR(duplicate)
ATTR2(0x0002ffff,type_int_end)
ATTR2(0x00030000,type_string_begin)
ATTR(type)
ATTR(label)
ATTR(data)
ATTR(charset)
ATTR(country_all)
ATTR(country_iso3)
ATTR(country_iso2)
ATTR(country_car)
ATTR(country_name)
ATTR(town_name)
ATTR(town_postal)
ATTR(district_name)
ATTR(street_name)
ATTR(street_name_systematic)
ATTR_UNUSED
ATTR(debug)
ATTR(address)
ATTR(phone)
ATTR(entry_fee)
ATTR(open_hours)
ATTR(skin)
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR(window_title)
ATTR_UNUSED
ATTR_UNUSED
/* poi */
ATTR_UNUSED
ATTR(info_html)
ATTR(price_html)
/* navigation */
ATTR(navigation_short)
ATTR(navigation_long)
ATTR(navigation_long_exact)
ATTR(navigation_speech)
ATTR(name)
ATTR(cursorname)
ATTR(source)
ATTR(description)
ATTR(gc_type)
ATTR_UNUSED
ATTR(position_nmea)
ATTR(gpsd_query)
ATTR(on_eof)
ATTR(command)
ATTR(src)
ATTR(path)
ATTR(font)
ATTR(url_local)
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR(icon_src)
ATTR(position_time_iso8601)
ATTR(house_number)
ATTR(osm_member)
ATTR(osm_tag)
ATTR(municipality_name)
ATTR(county_name)
ATTR(state_name)
ATTR(message)
ATTR_UNUSED
ATTR(enable_expression)
ATTR(fax)
ATTR(email)
ATTR(url)
ATTR(profilename)
ATTR(projectionname)
ATTR(town_or_district_name)
ATTR(postal)
ATTR(postal_mask)
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR_UNUSED
ATTR(town_name_match)
ATTR(district_name_match)
ATTR(street_name_match)
ATTR(language)
ATTR(subtype)
ATTR(filter)
ATTR(daylayout)
ATTR(nightlayout)
ATTR(xml_text)
ATTR(layout_name)
ATTR_UNUSED
ATTR_UNUSED
ATTR(status_text)
ATTR(log_gpx_desc)
ATTR(map_pass)
ATTR_UNUSED
ATTR(socket)
/* These attributes for house number interpolation are only written by
 * martin-s' (unpublished) GDF converter. */
ATTR(house_number_left)
ATTR(house_number_left_odd)
ATTR(house_number_left_even)
ATTR(house_number_right)
ATTR(house_number_right_odd)
ATTR(house_number_right_even)
ATTR(map_release)
ATTR(accesskey)
ATTR(http_method)
ATTR(http_header)
ATTR(progress)
ATTR(sample_dir)
ATTR(sample_suffix)
ATTR(dbus_destination)
ATTR(dbus_path)
ATTR(dbus_interface)
ATTR(dbus_method)
ATTR(osm_is_in)
ATTR(event_loop_system)
ATTR(map_name)
ATTR(item_name)
ATTR(state_file)
ATTR(on_map_click)
ATTR(route_depth)
ATTR(ref)
ATTR(tile_name)
ATTR(first_key)
ATTR(last_key)
ATTR(src_dir)
ATTR(refresh_cond)
/* House number interpolation information from OSM. For OSM data, the interpolation must
 * exclude the end nodes, because these are imported as separate nodes. */
ATTR(house_number_interpolation_no_ends_incrmt_1)
ATTR(house_number_interpolation_no_ends_incrmt_2)
ATTR(dbg_level)
ATTR(street_name_systematic_nat)
ATTR(street_name_systematic_int)
ATTR(street_destination)
ATTR(exit_to)
ATTR(street_destination_forward)
ATTR(street_destination_backward)
ATTR2(0x0003ffff,type_string_end)
ATTR2(0x00040000,type_special_begin)
ATTR(order)
ATTR(item_type)
ATTR(item_types)
ATTR(dash)
ATTR(sequence_range)
ATTR(angle_range)
ATTR(speed_range)
ATTR(attr_types)
ATTR(ch_edge)
ATTR(zipfile_ref_block)
ATTR(item_id)
ATTR(pdl_gps_update)
ATTR2(0x0004ffff,type_special_end)
ATTR2(0x00050000,type_double_begin)
ATTR(position_height)
ATTR(position_speed)
ATTR(position_direction)
ATTR(position_hdop)
ATTR(position_radius)
ATTR(position_longitude)
ATTR(position_latitude)
ATTR(position_direction_matched)
ATTR2(0x0005ffff,type_double_end)
ATTR2(0x00060000,type_coord_geo_begin)
ATTR(position_coord_geo)
ATTR(center)
ATTR(click_coord_geo)
ATTR2(0x0006ffff,type_coord_geo_end)
ATTR2(0x00070000,type_color_begin)
ATTR(color)
ATTR_UNUSED
ATTR(background_color)
ATTR(text_color)
ATTR(idle_color)
ATTR(background_color2)
ATTR(text_background)
ATTR2(0x0007ffff,type_color_end)
ATTR2(0x00080000,type_object_begin)
ATTR(navit)
ATTR(log)
ATTR(callback)
ATTR(route)
ATTR(navigation)
ATTR(vehicle)
ATTR(map)
ATTR(bookmark_map)
ATTR(bookmarks)
ATTR(former_destination_map)
ATTR(graphics)
ATTR(gui)
ATTR(trackingo) /* fixme */
ATTR(plugins)
ATTR(layer)
ATTR(itemgra)
ATTR(polygon)
ATTR(polyline)
ATTR(circle)
ATTR(text)
ATTR(icon)
ATTR(image)
ATTR(arrows)
ATTR(mapset)
ATTR(osd)
ATTR(plugin)
ATTR(speech)
ATTR(coord)
ATTR(private_data)
ATTR(callback_list)
ATTR(displaylist)
ATTR(transformation)
ATTR(vehicleprofile)
ATTR(roadprofile)
ATTR(announcement)
ATTR(cursor)
ATTR(config)
ATTR(maps)
ATTR(layout)
ATTR(profile_option)
ATTR(script)
ATTR2(0x0008ffff,type_object_end)
ATTR2(0x00090000,type_coord_begin)
ATTR2(0x0009ffff,type_coord_end)
ATTR2(0x000a0000,type_pcoord_begin)
ATTR(destination)
ATTR(position)
ATTR(position_test)
ATTR2(0x000affff,type_pcoord_end)
ATTR2(0x000b0000,type_callback_begin)
ATTR(resize)
ATTR(motion)
ATTR(keypress)
ATTR(window_closed)
ATTR(log_gpx)
ATTR(log_textfile)
ATTR(graphics_ready)
ATTR(destroy)
ATTR(wm_copydata)
ATTR2(0x000bffff,type_callback_end)
ATTR2(0x000c0000,type_int64_begin)
ATTR(osm_nodeid)
ATTR(osm_wayid)
ATTR(osm_relationid)
ATTR(osm_nodeid_first_node)
ATTR(osm_nodeid_last_node)
ATTR2(0x000cffff,type_int64_end)
ATTR2(0x000d0000,type_group_begin)
ATTR2(0x000dffff,type_group_end)
ATTR2(0x000e0000,type_item_type_begin)
ATTR2(0x000effff,type_item_type_end)
