/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2011 Navit Team
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
#include <string.h>
#include <stdlib.h>
#include "maptool.h"
#ifdef HAVE_POSTGRESQL
#ifndef _WIN32
#include <postgresql/libpq-fe.h>
#else
#include <libpq-fe.h>
#endif


/* Do some multipolygon processing
 * 
 * 
 * 
 */
int
multipolygon(PGconn *conn, struct maptool_osm *osm, char *tablename)
{

	PGresult *res, *node;
	PGresult *members;
	long lastrelation =-1;
	long way_id = 0;
	long lastnodeid = 0, firstnodeid = 0, nodeid = 0;
	int connected = FALSE;
	int i=0, countmembers = 0;
	char query[256];
	char curs[256];
	
	sprintf(curs,"declare members cursor for select id, relation_id, way_id, member_role from %s",tablename);
	res=PQexec(conn, curs);
	fprintf(stderr, "Cannot setup cursor for %s: %s\n", tablename, PQerrorMessage(conn));

	members=PQexec(conn, "fetch 10000 from members");
	if (! members) {
		fprintf(stderr, "Cannot setup cursor for ways: %s\n", PQerrorMessage(conn));
		PQclear(members);
		exit(1);
	}
	countmembers=PQntuples(members);
	fprintf(stderr, "fetch got %i members\n", countmembers);

	if (! countmembers)
		return 0;

	fprintf(stderr, "continue with %i members\n", countmembers);

	
	way_id = atol(PQgetvalue(members, i, 2));

	while (way_id > 0){
		long currrelation = atol(PQgetvalue(members, i, 1));
		int countnode = 0;
		int way_reverse = FALSE;
		fprintf(stderr, "75  member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
		
		if (lastrelation != currrelation && connected){
		//if (connected){ //also for relations with disjunct outers PQgetvalue(waterouter, i, 3)
		//if (connected && (strcmp(PQgetvalue(waterouter, i, 3), "inner")) != 0){ //also for relations with disjunct outers
			osm_add_tag("natural", "water");
			osm_end_way(osm);
			connected=FALSE;
			firstnodeid = 0;
			lastnodeid = 0;
			fprintf(stderr, "CONNECTED member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
			lastrelation = -1;
		}else{
			if (lastrelation > 0 && lastrelation != currrelation && !connected){
				firstnodeid = 0;
				lastnodeid = 0;
				lastrelation = -1;
				connected=FALSE;
				fprintf(stderr, "BOGUS member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
				}
			}

		if (lastrelation == -1){
			osm_add_way(0); //met welke id moet dit hier nu zijn ???
			processed_ways++;
			firstnodeid = 0;
			connected = FALSE;
			lastrelation = currrelation;
			}

		// nu iets voorzien om de eerste node te skippen als niet de eerste way ???

		sprintf(query,"select way_id,node_id from way_nodes where way_id = %ld order by sequence_id", way_id);
		node=PQexec(conn, query);
		if (! node) {
			fprintf(stderr, "Cannot query way_node: %s\n", PQerrorMessage(conn));
			exit(1);
		}

		countnode = PQntuples(node);
		fprintf(stderr, "fetch got %i nodes\n", countnode);

		// gesloten ways moeten nog uitgesloten worden ??
		if (lastnodeid == 0 && countmembers -1 > i ){
			// is next way in the same relation ?
			long oldrelation = atol(PQgetvalue(members, i, 1));
			long newrelation = atol(PQgetvalue(members, i + 1, 1));
			if( oldrelation == newrelation ){
				PGresult *nodenext;
				long nodeid_way_next = 0;
				long way_id_next = atol(PQgetvalue(members, i + 1, 2));
				sprintf(query,"select way_id,node_id from way_nodes where way_id = %ld order by sequence_id", way_id_next);
				nodenext=PQexec(conn, query);
				if (! nodenext) {
					fprintf(stderr, "Cannot query way_node: %s\n", PQerrorMessage(conn));
					exit(1);
				}
				nodeid = atoll(PQgetvalue(node, 0, 1));
				nodeid_way_next = atoll(PQgetvalue(nodenext, 0, 1));
				fprintf(stderr, "INVESTIGATE FIRST REVERSE WAY member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
				if (nodeid == nodeid_way_next){
					way_reverse = TRUE;
					fprintf(stderr, "FIRST REVERSE WAY member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
					}else{
						int countnodenext=PQntuples(nodenext);
						nodeid_way_next = atoll(PQgetvalue(nodenext, countnodenext -1, 1));
						if (nodeid == nodeid_way_next){
							way_reverse = TRUE;
							fprintf(stderr, "FIRST AND SECOND REVERSE WAY member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
							}
						}
				PQclear(nodenext);
				}
			}else{
				nodeid = atoll(PQgetvalue(node, 0, 1));
				if (!connected && lastnodeid != 0 && nodeid != lastnodeid ){
					fprintf(stderr, "INVESTIGATE REVERSE WAY member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
					nodeid = atoll(PQgetvalue(node, countnode - 1, 1));
					if (nodeid == lastnodeid){
						way_reverse = TRUE;
						fprintf(stderr, "REVERSE WAY member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
						}else{
							fprintf(stderr, "UNCONNECTED WAY member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
							}
					}
			}

		for (int j = 0 ; j < countnode ; j++) {
			// bij het beginnen van een nieuwe weg moet node[0] gelik zijn aan lastnode
			// anders in reverse proberen
			// sample relation 2388857
			if (!way_reverse){
				nodeid = atoll(PQgetvalue(node, j, 1));
			}else{
				nodeid = atoll(PQgetvalue(node, countnode - j -1 , 1));
			}
			if (currrelation == 6622088){
				fprintf(stderr, "nodeID = : %ld , lastnodeID =: %ld \n", nodeid, lastnodeid);
			}
			if (nodeid == firstnodeid){
				connected = TRUE;
				if (i < (countnode-1)){ // maybe there is another closed polygon in the rest of the nodes
					fprintf(stderr, "CONNECTED BUT MORE NODES member way = : %ld, relation = %ld, lastrelation = %ld\n", way_id, currrelation, lastrelation);
				//	osm_add_tag("natural", "water");
				//	osm_end_way(osm);
				//	connected=FALSE;
				//	firstnodeid = 0;
				//	lastnodeid = 0;
				//	osm_add_way(0); //met welke id moet dit hier nu zijn ???
				//	processed_ways++;
				}
			}else{if (firstnodeid == 0){
					firstnodeid = nodeid;
					}
			}
			if (nodeid != lastnodeid){ //skip a node for an outer made up of several connected ways
				osm_add_nd(nodeid);
				lastnodeid = nodeid;
			}
		}

		i++;
		if (i < countmembers){
			way_id = atol(PQgetvalue(members, i, 2));
			}else{
				way_id=-1;
				}
		}
		
		// om de laatste te sluiten
		if(connected){
			fprintf(stderr, "FINISH LAST MULTIPOLYGON member way = : %ld, lastrelation = %ld\n", way_id, lastrelation);
			osm_add_tag("natural", "water");
			osm_end_way(osm);
		}
		
		PQclear(members);
		PQclear(node);
	return 0;
}

int
map_collect_data_osm_db(char *dbstr, struct maptool_osm *osm)
{
	PGconn *conn;
	PGresult *res;
	char query[256];
	
	sig_alrm(0);
	conn=PQconnectdb(dbstr);
	if (! conn) {
		fprintf(stderr,"Failed to connect to database with '%s'\n",dbstr);
		exit(1);
	}
	fprintf(stderr,"connected to database with '%s'\n",dbstr);
	res=PQexec(conn, "begin");
	if (! res) {
		fprintf(stderr, "Cannot begin transaction: %s\n", PQerrorMessage(conn));
		PQclear(res);
		exit(1);
	}
	res=PQexec(conn, "set transaction isolation level serializable");
	if (! res) {
		fprintf(stderr, "Cannot set isolation level: %s\n", PQerrorMessage(conn));
		PQclear(res);
		exit(1);
	}
	res=PQexec(conn, "declare nodes cursor for select id, ST_Y(geom), ST_X(geom) from nodes order by id");
	if (! res) {
		fprintf(stderr, "Cannot setup cursor for nodes: %s\n", PQerrorMessage(conn));
		PQclear(res);
		exit(1);
	}
	res=PQexec(conn, "declare ways cursor for select id from ways order by id");
	if (! res) {
		fprintf(stderr, "Cannot setup cursor for ways: %s\n", PQerrorMessage(conn));
		PQclear(res);
		exit(1);
	}
	res=PQexec(conn, "declare relations cursor for select id from relations order by id");
	if (! res) {
		fprintf(stderr, "Cannot setup cursor for relations: %s\n", PQerrorMessage(conn));
		PQclear(res);
		exit(1);
	}

	for (;;) {
		int j=0, count=0;
		long min, max, id, tag_id;
		PGresult *node, *tag;
		node=PQexec(conn, "fetch 100000 from nodes");
		if (! node) {
			fprintf(stderr, "Cannot setup cursor for nodes: %s\n", PQerrorMessage(conn));
			PQclear(node);
			exit(1);
		}
		count=PQntuples(node);
		if (! count)
			break;
		min=atol(PQgetvalue(node, 0, 0));
		max=atol(PQgetvalue(node, count-1, 0));
		sprintf(query,"select node_id,k,v from node_tags where node_id >= %ld and node_id <= %ld order by node_id", min, max);
		tag=PQexec(conn, query);
		if (! tag) {
			fprintf(stderr, "Cannot query node_tag: %s\n", PQerrorMessage(conn));
			exit(1);
		}
		fprintf(stderr, "query node_tag got : %i tuples\n", PQntuples(tag));
		for (int i = 0 ; i < count ; i++) {
			id=atol(PQgetvalue(node, i, 0));
			osm_add_node(id, atof(PQgetvalue(node, i, 1)), atof(PQgetvalue(node, i, 2)));
			processed_nodes++;
			while (j < PQntuples(tag)) {
				tag_id=atol(PQgetvalue(tag, j, 0));
				if (tag_id == id) {
					osm_add_tag(PQgetvalue(tag, j, 1), PQgetvalue(tag, j, 2));
					j++;
				}
				if (tag_id < id)
					j++;
				if (tag_id > id)
					break;
			}
			osm_end_node(osm);
		}
		PQclear(tag);
		PQclear(node);
	}
	
	for (;;) {
		int j=0, k=0, count=0, tagged=0;
		long min, max, id, tag_id, node_id;
		PGresult *node,*way,*tag;
		way=PQexec(conn, "fetch 50000 from ways");
		if (! way) {
			fprintf(stderr, "Cannot setup cursor for ways: %s\n", PQerrorMessage(conn));
			PQclear(way);
			exit(1);
		}
		count=PQntuples(way);
		if (! count)
			break;
		min=atol(PQgetvalue(way, 0, 0));
		max=atol(PQgetvalue(way, count-1, 0));
		sprintf(query,"select way_id,node_id from way_nodes where way_id >= %ld and way_id <= %ld order by way_id,sequence_id", min, max);
		node=PQexec(conn, query);
		if (! node) {
			fprintf(stderr, "Cannot query way_node: %s\n", PQerrorMessage(conn));
			exit(1);
		}
		sprintf(query,"select way_id,k,v from way_tags where way_id >= %ld and way_id <= %ld order by way_id", min, max);
		tag=PQexec(conn, query);
		if (! tag) {
			fprintf(stderr, "Cannot query way_tag: %s\n", PQerrorMessage(conn));
			exit(1);
		}
		for (int i = 0 ; i < count ; i++) {
			id=atol(PQgetvalue(way, i, 0));
			osm_add_way(id);
			tagged=0;
			processed_ways++;
			while (k < PQntuples(node)) {
				node_id=atol(PQgetvalue(node, k, 0));
				if (node_id == id) {
					osm_add_nd(atoll(PQgetvalue(node, k, 1)));
					tagged=1;
					k++;
				}
				if (node_id < id)
					k++;
				if (node_id > id)
					break;
			}
			while (j < PQntuples(tag)) {
				tag_id=atol(PQgetvalue(tag, j, 0));
				if (tag_id == id) {
					osm_add_tag(PQgetvalue(tag, j, 1), PQgetvalue(tag, j, 2));
					tagged=1;
					j++;
				}
				if (tag_id < id)
					j++;
				if (tag_id > id)
					break;
			}
			if (tagged)
				osm_end_way(osm);
		}
		PQclear(tag);
		PQclear(node);
		PQclear(way);
	}
	
	
	multipolygon(conn, osm,"water");
	
	
	for (;;) {
		int j=0, k=0, count=0, tagged=0;
		long min, max, id;
		PGresult *tag, *relation, *member;
		relation=PQexec(conn, "fetch 40000 from relations");
		if (! relation) {
			fprintf(stderr, "Cannot setup cursor for relations: %s\n", PQerrorMessage(conn));
			PQclear(relation);
			exit(1);
		}
		count=PQntuples(relation);
		fprintf(stderr, "Got %i relations\n", count);
		if (! count)
			break;
		min=atol(PQgetvalue(relation, 0, 0));
		max=atol(PQgetvalue(relation, count-1, 0));
		sprintf(query,"select relation_id,k,v from relation_tags where relation_id >= %ld and relation_id <= %ld order by relation_id", min, max);
		tag=PQexec(conn, query);
		if (! tag) {
			fprintf(stderr, "Cannot query relation_tag: %s\n", PQerrorMessage(conn));
			exit(1);
		}
		sprintf(query,"select relation_id, member_id, member_type, member_role from relation_members where relation_id >= %ld and relation_id <= %ld order by relation_id, sequence_id", min, max);
		member=PQexec(conn, query);
		if (! member) {
			fprintf(stderr, "Cannot query relation_members: %s\n", PQerrorMessage(conn));
			exit(1);
		}
		for (int i = 0 ; i < count ; i++) {
			id=atol(PQgetvalue(relation, i, 0));
			osm_add_relation(id);
			tagged = 0;
			while (j < PQntuples(tag)) {
				long tag_relation_id=atol(PQgetvalue(tag, j, 0));
				if (tag_relation_id == id) {
					osm_add_tag(PQgetvalue(tag, j, 1), PQgetvalue(tag, j, 2));
					tagged=1;
					j++;
				}
				if (tag_relation_id < id)
					j++;
				if (tag_relation_id > id)
					break;
			}
			while (k < PQntuples(member)) {
				long member_relation_id=atol(PQgetvalue(member, k, 0));
				if (member_relation_id == id) {
					int relmember_type=0; //type unknown
					if (!strcmp(PQgetvalue(member,k, 2),"W")){
						relmember_type=2;
						}else{
								if (!strcmp(PQgetvalue(member,k, 2),"N")){
								relmember_type=1;
								}else{
									if (!strcmp(PQgetvalue(member,k, 2),"R")){
										relmember_type=3;
										}
									}
							}
					osm_add_member(relmember_type,atoll(PQgetvalue(member,k, 1)),PQgetvalue(member,k, 3));
					k++;
				}
				if (member_relation_id < id)
					k++;
				if (member_relation_id > id)
					break;
			}
			if (tagged)
				osm_end_relation(osm);
		}
		PQclear(relation);
		PQclear(member);
		PQclear(tag);
	}

	res=PQexec(conn, "commit");
	if (! res) {
		fprintf(stderr, "Cannot commit transaction: %s\n", PQerrorMessage(conn));
		PQclear(res);
		exit(1);
	}
	sig_alrm(0);
	sig_alrm_end();
	return 1;
}
#endif
