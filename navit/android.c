#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "android.h"
#include <android/log.h>
#include "navit.h"
#include "config_.h"
#include "command.h"
#include "debug.h"
#include "event.h"
#include "callback.h"
#include "country.h"
#include "projection.h"
#include "map.h"
#include "mapset.h"
#include "navit_nls.h"
#include "transform.h"
#include "color.h"
#include "types.h"
#include "search.h"
#include "start_real.h"
#include "track.h"

JNIEnv *jnienv_t;
JNIEnv *jnienv;
JNIEnv *jnienv2;
jobject *android_activity;
JavaVM *cachedJVM = NULL;

struct android_search_priv
{
    struct jni_object search_result_obj;
    struct event_idle *idle_ev;
    struct callback *idle_clb;
    struct search_list *search_list;
    struct attr search_attr;
    gchar **phrases;
    int current_phrase_per_level[4];
    int partial;
    int found;
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *aVm, void *aReserved)
{
    cachedJVM = aVm;
    if ((*aVm)->GetEnv(aVm,(void**)&jnienv, JNI_VERSION_1_6) != JNI_OK)
     {
      dbg(lvl_error,"Failed to get the environment");
      return -1;
     }
        dbg(lvl_debug,"Found the environment");
    return JNI_VERSION_1_6;
}


JNIEnv* jni_getenv()
{
    JNIEnv* env_this;
    (*cachedJVM)->GetEnv(cachedJVM, (void**) &env_this, JNI_VERSION_1_6);
    return env_this;
}


int
android_find_class_global(char *name, jclass *ret)
{
    jnienv2 = jni_getenv();
    *ret=(*jnienv2)->FindClass(jnienv2, name);
    if (! *ret) {
        dbg(lvl_error,"Failed to get Class %s\n",name);
        return 0;
    }
    *ret = (*jnienv2)->NewGlobalRef(jnienv2, *ret);
    return 1;
}

int
android_find_method(jclass class, char *name, char *args, jmethodID *ret)
{
    jnienv2 = jni_getenv();
    *ret = (*jnienv2)->GetMethodID(jnienv2, class, name, args);
    if (*ret == NULL) {
        dbg(lvl_error,"Failed to get Method %s with signature %s\n",name,args);
        return 0;
    }
    return 1;
}


int
android_find_static_method(jclass class, char *name, char *args, jmethodID *ret)
{
    jnienv2 = jni_getenv();
    *ret = (*jnienv2)->GetStaticMethodID(jnienv2, class, name, args);
    if (*ret == NULL) {
        dbg(lvl_error,"Failed to get static Method %s with signature %s\n",name,args);
        return 0;
    }
    return 1;
}


JNIEXPORT void JNICALL
Java_org_navitproject_navit_Navit_navitMain(JNIEnv *env, jobject thiz, jstring lang, jstring path,
                                            jstring mappath)
{
    const char *langstr;
    const char *displaydensitystr;
    const char *map_filename_path;
    jnienv_t=env;
    android_activity = (*env)->NewGlobalRef(env, thiz);
    langstr=(*env)->GetStringUTFChars(env, lang, NULL);
    dbg(lvl_debug,"enter env=%p activity=%p lang=%s\n",env,android_activity,langstr);
    setenv("LANG",langstr,1);
    (*env)->ReleaseStringUTFChars(env, lang, langstr);

    map_filename_path=(*env)->GetStringUTFChars(env, mappath, NULL);
    setenv("NAVIT_USER_DATADIR",map_filename_path,1);
    (*env)->ReleaseStringUTFChars(env, mappath, map_filename_path);

    const char *strings=(*env)->GetStringUTFChars(env, path, NULL);
    main_real(1, &strings);
    (*env)->ReleaseStringUTFChars(env, path, strings);
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_Navit_navitDestroy(JNIEnv *env, jobject instance)
{
    dbg(lvl_debug, "shutdown navit\n");
    exit(0);
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitGraphics_sizeChangedCallback( JNIEnv* env, jobject thiz, jlong id, jint w, jint h)
{
    dbg(lvl_debug,"enter %p %d %d\n",(struct callback *)(intptr_t)id,w,h);
    if (id) {
        callback_call_2((struct callback *) (intptr_t)id, w, h);
    }
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitGraphics_buttonCallback( JNIEnv* env, jobject thiz, jlong id, jint pressed, jint button, jint x, jint y)
{
    dbg(lvl_debug,"enter %p %d %d\n",(struct callback *)(intptr_t)id,pressed,button);
    if (id) {
        callback_call_4((struct callback *)(intptr_t) id, pressed, button, x, y);
    }
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitGraphics_motionCallback( JNIEnv* env, jobject thiz, jlong id, jint x, jint y)
{
    dbg(lvl_debug,"enter %p %d %d\n",(struct callback *)(intptr_t)id,x,y);
    if (id) {
        callback_call_2((struct callback *) (intptr_t)id, x, y);
    }
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitGraphics_keypressCallback( JNIEnv* env, jobject thiz, jlong id, jstring str)
{
    const char *s;
    dbg(lvl_debug,"enter %p %p\n",(struct callback *)(intptr_t)id,str);
    s=(*env)->GetStringUTFChars(env, str, NULL);
    dbg(lvl_debug,"key = %s",s);
    if (id) {
        callback_call_1((struct callback *) (intptr_t)id, s);
    }
    (*env)->ReleaseStringUTFChars(env, str, s);
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitTimeout_timeoutCallback(JNIEnv *env, jobject instance, jlong id)
{
    dbg(lvl_debug,"enter %p %p\n",instance, (void *)(intptr_t)id);
    void (*event_handler)(void *) = *(void **)(intptr_t)id;
    
    event_handler((void*)(intptr_t)id);
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitVehicle_vehicleCallback( JNIEnv * env, jobject thiz, jlong id, jobject location)
{
    callback_call_1((struct callback *)(intptr_t)id, (void *)location);
}


JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitIdle_IdleCallback( JNIEnv* env, jobject thiz, jlong id)
{
    dbg(lvl_debug,"enter %p %p\n",thiz, (void *)(intptr_t)id);
    callback_call_0((struct callback *)(intptr_t)id);
}

JNIEXPORT void JNICALL Java_org_navitproject_navit_NavitWatch_poll(JNIEnv *env, jobject instance,
                                                jlong func,
                                                jint fd,
                                                jint cond)
{
    void (*pollfunc)(JNIEnv *env, int fd, int cond)=(void *)func;

    pollfunc(env, fd, cond);
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitWatch_watchCallback(JNIEnv *env, jobject instance, jlong id)
{

  dbg(lvl_debug,"enter %p %p\n",instance, (void *)(intptr_t)id);
    callback_call_0((struct callback *)(intptr_t)id);

}


JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitSensors_sensorCallback( JNIEnv* env, jobject thiz, jlong id, jint sensor, jfloat x, jfloat y, jfloat z)
{
    dbg(lvl_debug,"enter %p %p %f %f %f\n",thiz, (void *)(intptr_t)id,x,y,z);
    callback_call_4((struct callback *)(intptr_t)id, sensor, &x, &y, &z);
}

// type: 0=town, 1=street, 2=House#
void
android_return_search_result(struct jni_object *jni_o, int type, int id, struct pcoord *location, const char *address, const char *address_extras)
{
    struct coord_geo geo_location;
    struct coord c;
    jstring jaddress = NULL;
    jstring jaddress_extras = NULL;
    JNIEnv* env;
    env=jni_o->env;
    jaddress = (*env)->NewStringUTF(jni_o->env,address);
    jaddress_extras = (*env)->NewStringUTF(jni_o->env,address_extras);

//  dbg(0,"address =%s, extras = %s\n",address,address_extras);

    c.x=location->x;
    c.y=location->y;
    transform_to_geo(location->pro, &c, &geo_location);

    (*env)->CallVoidMethod(jni_o->env, jni_o->jo, jni_o->jm, type, id, geo_location.lat, geo_location.lng, jaddress, jaddress_extras);
    (*env)->DeleteLocalRef(jni_o->env, jaddress);
    (*env)->DeleteLocalRef(jni_o->env, jaddress_extras);
}

JNIEXPORT jobject JNICALL
Java_org_navitproject_navit_CallBackHandler_callbackCmdChannel( JNIEnv* env, jclass thiz, jint command)
{
    struct attr attr;
    const char *s;
    dbg(lvl_debug,"enter %d\n",command);
    config_get_attr(config_get(), attr_navit, &attr, NULL);

    switch(command)
    {
        case 1:
            // zoom in
            navit_zoom_in_cursor(attr.u.navit, 2);
            navit_draw(attr.u.navit);
            break;
        case 2:
            // zoom out
            navit_zoom_out_cursor(attr.u.navit, 2);
            navit_draw(attr.u.navit);
            break;
        case 3:
            // block
            navit_block(attr.u.navit, 1);
            break;
        case 4:
            // unblock
            navit_block(attr.u.navit, 0);
            break;
        case 5:
            // cancel route
            navit_set_destination(attr.u.navit, NULL, NULL, 0);
            navit_draw(attr.u.navit);
            break;
        default:
            dbg(lvl_error, "Unknown command: %d", command);
            break;
    }
}

JNIEXPORT jobject JNICALL
Java_org_navitproject_navit_CallBackHandler_callbackMessageChannel( JNIEnv* env, jclass thiz, jint channel, jstring str)
{
    struct attr attr;
    const char *s;
    dbg(lvl_debug,"enter %d %p\n",channel,str);

    config_get_attr(config_get(), attr_navit, &attr, NULL);

    switch(channel)
    {
    case 3:
        {
            // navigate to geo position
            char *name;
            s=(*env)->GetStringUTFChars(env, str, NULL);
            char parse_str[strlen(s) + 1];
            strcpy(parse_str, s);
            (*env)->ReleaseStringUTFChars(env, str, s);
            dbg(lvl_error,"*****string=%s\n",s);

            // passing an empty string cancels navigation
            if (strlen(parse_str) == 0){
                navit_set_destination(attr.u.navit, NULL, NULL, 0);
            } else {
                // set destination to (lat#lon#title)
                struct coord_geo g;
                char *p;
                char *stopstring;

                // lat
                p = strtok (parse_str,"#");
                g.lat = strtof(p, &stopstring);
                // lon
                p = strtok (NULL, "#");
                g.lng = strtof(p, &stopstring);
                // description
                name = strtok (NULL, "#");

                dbg(lvl_error,"lat=%f\n",g.lat);
                dbg(lvl_error,"lng=%f\n",g.lng);
                dbg(lvl_error,"str1=%s\n",name);

                struct coord c;
                transform_from_geo(projection_mg, &g, &c);

                struct pcoord pc;
                pc.x=c.x;
                pc.y=c.y;
                pc.pro=projection_mg;

                // start navigation asynchronous
                navit_set_destination(attr.u.navit, &pc, name, TRUE);
            }
        }
        break;
    case 4:
        {
            // navigate to display position
            char *pstr;
            struct point p;
            struct coord c;
            struct pcoord pc;

            struct transformation *transform=navit_get_trans(attr.u.navit);

            s=(*env)->GetStringUTFChars(env, str, NULL);
            char parse_str[strlen(s) + 1];
            strcpy(parse_str, s);
            (*env)->ReleaseStringUTFChars(env, str, s);
            dbg(lvl_debug,"*****string=%s\n",parse_str);

            // set destination to (pixel-x#pixel-y)
            // pixel-x
            pstr = strtok (parse_str,"#");
            p.x = atoi(pstr);
            // pixel-y
            pstr = strtok (NULL, "#");
            p.y = atoi(pstr);

            dbg(lvl_debug,"11x=%d\n",p.x);
            dbg(lvl_debug,"11y=%d\n",p.y);

            transform_reverse(transform, &p, &c);

            pc.x = c.x;
            pc.y = c.y;
            pc.pro = transform_get_projection(transform);

            dbg(lvl_debug,"22x=%d\n",pc.x);
            dbg(lvl_debug,"22y=%d\n",pc.y);

            // start navigation asynchronous
            navit_set_destination(attr.u.navit, &pc, parse_str, TRUE);
        }
        break;

    case 5:
        // call a command (like in gui)
        s=(*env)->GetStringUTFChars(env, str, NULL);
        dbg(lvl_debug,"*****string=%s\n",s);
        command_evaluate(&attr,s);
        (*env)->ReleaseStringUTFChars(env, str, s);
        break;
    case 6: // add a map to the current mapset, return 1 on success
    {
        struct mapset *ms = navit_get_mapset(attr.u.navit);
        struct attr type, name, data, *attrs[4];
        const char *map_location=(*env)->GetStringUTFChars(env, str, NULL);
        dbg(lvl_debug,"*****string=%s\n",map_location);
        type.type=attr_type;
        type.u.str="binfile";

        data.type=attr_data;
        data.u.str=g_strdup(map_location);

        name.type=attr_name;
        name.u.str=g_strdup(map_location);

        attrs[0]=&type; attrs[1]=&data; attrs[2]=&name; attrs[3]=NULL;

        struct map * new_map = map_new(NULL, attrs);
        if (new_map) {
            struct attr map_a;
            map_a.type=attr_map;
            map_a.u.map=new_map;
            mapset_add_attr(ms, &map_a);
            navit_draw(attr.u.navit);
        }
        (*env)->ReleaseStringUTFChars(env, str, map_location);
    }
    break;
    case 7: // remove a map from the current mapset
    {
        struct mapset *ms = navit_get_mapset(attr.u.navit);
        struct attr map_r;
        const char *map_location=(*env)->GetStringUTFChars(env, str, NULL);
        struct map * delete_map = mapset_get_map_by_name(ms, map_location);
        if (delete_map) {
            dbg(lvl_debug,"delete map %s (%p)", map_location, delete_map);
            map_r.type=attr_map;
            map_r.u.map=delete_map;
            mapset_remove_attr(ms, &map_r);
            navit_draw(attr.u.navit);
        }
        (*env)->ReleaseStringUTFChars(env, str, map_location);
    }
    break;
    default:
        dbg(lvl_error, "Unknown message: %d", channel);
        break;
    }

    return NULL;
}

// int channel not used ?
JNIEXPORT jstring JNICALL
Java_org_navitproject_navit_NavitGraphics_getDefaultCountry( JNIEnv* env, jobject thiz, jint channel, jstring str)
{
    struct attr search_attr, country_name, country_iso2, *country_attr;
    struct tracking *tracking;
    struct search_list_result *res;
    jstring return_string = NULL;

    struct attr attr;
    dbg(lvl_debug,"enter %d %p\n",channel,str);

    config_get_attr(config_get(), attr_navit, &attr, NULL);

    country_attr=country_default();
    tracking=navit_get_tracking(attr.u.navit);
    if (tracking && tracking_get_attr(tracking, attr_country_id, &search_attr, NULL)) {
        country_attr = &search_attr;
    }
    if (country_attr) {
        struct country_search *cs=country_search_new(country_attr, 0);
        struct item *item=country_search_get_item(cs);
        if (item && item_attr_get(item, attr_country_name, &country_name)) {
            struct mapset *ms=navit_get_mapset(attr.u.navit);
            struct search_list *search_list = search_list_new(ms);
            search_attr.type=attr_country_all;
            dbg(lvl_debug,"country %s\n", country_name.u.str);
            search_attr.u.str=country_name.u.str;
            search_list_search(search_list, &search_attr, 0);
            while((res=search_list_get_result(search_list))) {
                dbg(lvl_debug,"Get result: %s\n", res->country->iso2);
            }
            if (item_attr_get(item, attr_country_iso2, &country_iso2)) {
                return_string = (*env)->NewStringUTF(env, country_iso2.u.str);
            }
        }
        country_search_destroy(cs);
    }
    return return_string;
}

JNIEXPORT jobjectArray JNICALL
Java_org_navitproject_navit_NavitAddressSearchActivity_getAllCountries( JNIEnv* env, jclass thiz)
{
    struct attr search_attr;
    struct search_list_result *res;
    GList* countries = NULL;
    int country_count = 0;
    jobjectArray all_countries;

    struct attr attr;
    dbg(lvl_debug,"enter\n");

    config_get_attr(config_get(), attr_navit, &attr, NULL);

    struct mapset *ms=navit_get_mapset(attr.u.navit);
    struct search_list *search_list = search_list_new(ms);
    jobjectArray current_country = NULL;
    search_attr.type=attr_country_all;
    //dbg(lvl_debug,"country %s\n", country_name.u.str);
    search_attr.u.str=g_strdup("");//country_name.u.str;
    search_list_search(search_list, &search_attr, 1);
    while((res=search_list_get_result(search_list))) {
        dbg(lvl_debug,"Get result: %s\n", res->country->iso2);

        if (strlen(res->country->iso2)==2) {
            jstring j_iso2 = (*env)->NewStringUTF(env, res->country->iso2);
            jstring j_name = (*env)->NewStringUTF(env, navit_nls_gettext(res->country->name));

            current_country = (jobjectArray)(*env)->NewObjectArray(env, 2, (*env)->FindClass(env, "java/lang/String"), NULL);

            (*env)->SetObjectArrayElement(env, current_country, 0,  j_iso2);
            (*env)->SetObjectArrayElement(env, current_country, 1,  j_name);

            (*env)->DeleteLocalRef(env, j_iso2);
            (*env)->DeleteLocalRef(env, j_name);

            countries = g_list_prepend(countries, current_country);
            country_count++;
        }
    }

    search_list_destroy(search_list);
    all_countries = (jobjectArray)(*env)->NewObjectArray(env, country_count, (*env)->GetObjectClass(env, current_country), NULL);

    while(countries) {
        (*env)->SetObjectArrayElement(env, all_countries, --country_count, countries->data);
        countries = g_list_delete_link( countries, countries);
    }
    return all_countries;
}

static char *
postal_str(struct search_list_result *res, int level)
{
    char *ret=NULL;
    if (res->town->common.postal) {
        ret = res->town->common.postal;
    }
    if (res->town->common.postal_mask) {
        ret = res->town->common.postal_mask;
    }
    if (level == 1) {
        return ret;
    }
    if (res->street->common.postal) {
        ret = res->street->common.postal;
    }
    if (res->street->common.postal_mask) {
        ret = res->street->common.postal_mask;
    }
    if (level == 2) {
        return ret;
    }
    if (res->house_number->common.postal) {
        ret = res->house_number->common.postal;
    }
    if (res->house_number->common.postal_mask) {
        ret = res->house_number->common.postal_mask;
    }
    return ret;
}

static char *
district_str(struct search_list_result *res, int level)
{
    char *ret=NULL;
    if (res->town->common.district_name) {
        ret = res->town->common.district_name;
    }
    if (level == 1) {
        return ret;
    }
    if (res->street->common.district_name) {
        ret = res->street->common.district_name;
    }
    if (level == 2) {
        return ret;
    }
    if (res->house_number->common.district_name) {
        ret = res->house_number->common.district_name;
    }
    return ret;
}

static char *
town_str(struct search_list_result *res, int level){
    char *district = district_str(res, level % 10);
    char *postal = postal_str(res, level % 10);
    char *postal_sep = " ";
    char *district_begin = " (";
    char *district_end = ")";
    char *county_sep = " ";
    char *county = res->town->common.county_name;
    if (!postal) {
    postal_sep = postal = "";
    }
    if (!district) {
        district_begin = district_end = district = "";
    }
    if (!county){
        county_sep = county = "";
    }
    if (level == 11){
        char *town=res->town->common.town_name;
        return g_strdup_printf("%s", town);
    } else{
        return g_strdup_printf("%s%s%s%s%s%s%s", postal, postal_sep, district_begin, district, district_end, county_sep, county);
    }
}

static void
android_search_end(struct android_search_priv *search_priv)
{
    dbg(lvl_debug, "End search");
    JNIEnv* env = search_priv->search_result_obj.env;
    if (search_priv->idle_ev) {
        event_remove_idle(search_priv->idle_ev);
        search_priv->idle_ev=NULL;
    }
    if (search_priv->idle_clb) {
        callback_destroy(search_priv->idle_clb);
        search_priv->idle_clb=NULL;
    }
    jclass cls = (*env)->GetObjectClass(env,search_priv->search_result_obj.jo);
    jmethodID finish_MethodID = (*env)->GetMethodID(env, cls, "finishAddressSearch", "()V");
    if(finish_MethodID != 0) {
        (*env)->CallVoidMethod(env, search_priv->search_result_obj.jo, finish_MethodID);
    } else {
        dbg(lvl_error, "Error method finishAddressSearch not found");
    }

//  voorlopig dan maar niet
//  search_list_destroy(search_priv->search_list);
//  g_strfreev(search_priv->phrases);
//  g_free(search_priv);
}


static void
android_search_idle(struct android_search_priv *search_priv)
{
    dbg(lvl_debug, "enter android_search_idle search type =%s\n",attr_to_name(search_priv->search_attr.type));

    struct search_list_result *res = search_list_get_result(search_priv->search_list);
    if (res) {
        dbg(lvl_debug, "Town: %s, Street: %s\n",res->town ? res->town->common.town_name : "no town", res->street ? res->street->name : "no street");
        search_priv->found = 1;
        switch (search_priv->search_attr.type) {
            case attr_town_or_district_name:
            case attr_town_postal: {
                gchar *town = town_str(res, 11);
                gchar *town_extras = town_str(res, 1);
                android_return_search_result(&search_priv->search_result_obj, 0, res->id,
                                             res->town->common.c, town, town_extras);
                g_free(town);
                g_free(town_extras);
                break;
            }

                // town weergave  zal nog moeten bijgewerkt worden
            case attr_street_name: {
                gchar *town = town_str(res, 2);
                gchar *address = g_strdup_printf("%.101s,%.101s", res->country->name, town);
                gchar *street_name = g_strdup_printf("%.101s", res->street->name);
                android_return_search_result(&search_priv->search_result_obj, 1, res->id,
                                             res->street->common.c, street_name, address);
                g_free(address);
                g_free(town);
                break;
            }
            case attr_house_number: {
                gchar *town = town_str(res, 3);
                gchar *address = g_strdup_printf("%.101s, %.101s, %.101s", res->country->name, town,
                                                 res->street->name);
                gchar *housenumber = g_strdup_printf("%.15s", res->house_number->house_number);
                android_return_search_result(&search_priv->search_result_obj, 2, res->id,
                                             res->house_number->common.c, housenumber, address);
                g_free(address);
                g_free(town);
                break;
            }
            default:
                dbg(lvl_error, "Unhandled search type %d", search_priv->search_attr.type);
        }
    } else {
        android_search_end(search_priv);
    }
}


static char *
search_fix_spaces(const char *str)
{
    int i;
    int len=strlen(str);
    char c,*s,*d,*ret=g_strdup(str);

    for (i = 0 ; i < len ; i++) {
        if (ret[i] == ',' || ret[i] == '/')
            ret[i]=' ';
    }
    s=ret;
    d=ret;
    len=0;
    do {
        c=*s++;
        if (c != ' ' || len != 0) {
            *d++=c;
            len++;
        }
        while (c == ' ' && *s == ' ')
            s++;
        if (c == ' ' && *s == '\0') {
            d--;
            len--;
        }
    } while (c);
    return ret;
}

static void start_search(struct android_search_priv *search_priv, const char *search_string, int type)
{
    char *str=search_fix_spaces(search_string);
    search_priv->phrases = g_strsplit(str, "#", 0);

    // later kan hieronder gewoon str gebruikt worden en dat splitten laten

    search_priv->search_attr.u.str= search_priv->phrases[0];
    search_priv->search_attr.type=attr_town_or_district_name;
    if (search_string[0] >= '0' && search_string[0] <= '9'){
        search_priv->search_attr.type=attr_town_postal;
    }
//  if (!search_priv->phrases[1] ) // blijkbaar zijn we nog town aan het zoeken
    if (type == 2) {// blijkbaar zijn we nog town aan het zoeken
        search_list_search(search_priv->search_list, &search_priv->search_attr, search_priv->partial);
        search_priv->idle_clb = callback_new_1(callback_cast(android_search_idle), search_priv);
        search_priv->idle_ev = event_add_idle(50,search_priv->idle_clb);
        //callback_call_0(search_priv->idle_clb);
    } else {
        if (type == 3){ // blijkbaar zijn we nog street aan het zoeken
                search_priv->search_attr.type=attr_street_name;
                search_list_search(search_priv->search_list, &search_priv->search_attr, search_priv->partial);
                search_priv->idle_clb = callback_new_1(callback_cast(android_search_idle), search_priv);
                search_priv->idle_ev = event_add_idle(50,search_priv->idle_clb);
                //callback_call_0(search_priv->idle_clb);
            } else if (type == 4) {// nu huisnummer zoeken
                        search_priv->search_attr.type=attr_house_number;
                        search_list_search(search_priv->search_list, &search_priv->search_attr, search_priv->partial);
                        search_priv->idle_clb = callback_new_1(callback_cast(android_search_idle), search_priv);
                        search_priv->idle_ev = event_add_idle(50,search_priv->idle_clb);
                        //callback_call_0(search_priv->idle_clb);
                    }
    }
    g_free(str);
    dbg(lvl_debug,"leave\n");
}

JNIEXPORT jlong JNICALL
Java_org_navitproject_navit_NavitAddressSearchActivity_callbackStartAddressSearch( JNIEnv* env, jobject thiz, jint partial, jstring country, jstring str)
{

    struct attr attr;
    const char *search_string =(*env)->GetStringUTFChars(env, str, NULL);
    dbg(lvl_debug,"search '%s'\n", search_string);

    config_get_attr(config_get(), attr_navit, &attr, NULL);

    jclass cls = (*env)->GetObjectClass(env,thiz);
    jmethodID aMethodID = (*env)->GetMethodID(env, cls, "receiveAddress", "(IIFFLjava/lang/String;Ljava/lang/String;)V");

    struct android_search_priv *search_priv = NULL;

    if(aMethodID != 0) {
        struct mapset *ms4=navit_get_mapset(attr.u.navit);
        struct attr country_attr;
        const char *str_country=(*env)->GetStringUTFChars(env, country, NULL);
        struct search_list_result *slr;
        int count = 0;

        search_priv = g_new0( struct android_search_priv, 1);
        search_priv->search_list = search_list_new(ms4);
        search_priv->partial = partial;
        search_priv->current_phrase_per_level[1] = -1;
        search_priv->current_phrase_per_level[2] = -1;

        country_attr.type=attr_country_iso2;
        country_attr.u.str=g_strdup(str_country);
        search_list_search(search_priv->search_list, &country_attr, 0);

        while ((slr=search_list_get_result(search_priv->search_list))) {
            count++;
        }
        if (!count) {
            dbg(lvl_error, "Country not found");
        }

        dbg(lvl_debug,"search in country '%s'\n", str_country);
        (*env)->ReleaseStringUTFChars(env, country, str_country);

        search_priv->search_result_obj.env = env;
        search_priv->search_result_obj.jo = (*env)->NewGlobalRef(env, thiz);
        search_priv->search_result_obj.jm = aMethodID;
    } else {
        dbg(lvl_error, "**** Unable to get methodID: receiveAddress\n");
    }

    (*env)->ReleaseStringUTFChars(env, str, search_string);

    return (jlong)(long)search_priv;
}

JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitAddressSearchActivity_callbackCancelAddressSearch( JNIEnv* env, jobject thiz, jlong handle)
{
    struct android_search_priv *priv = (void*)(long)handle;

    if (priv) {
        android_search_end(priv);
    } else {
        dbg(lvl_error, "Error: Cancel search failed");
    }
}

// aanroep met type en string is zoeken
// aanroep met type en id is selecteren om vervolgens een level dieper te gaan zoeken
JNIEXPORT void JNICALL
Java_org_navitproject_navit_NavitAddressSearchActivity_callbackSearch(JNIEnv *env,
                                                                      jobject instance,
                                                                      jlong handle,
                                                                      jint type,
                                                                      jint id,
                                                                      jstring str_) {

    struct android_search_priv *priv = (void*)(long)handle;
    const char *search_string =(*env)->GetStringUTFChars(env, str_, NULL);

    if (priv){
        if (id){
            if (type==2){ // 2 = town in android
                search_list_select(priv->search_list, attr_town_or_district_name, 0, 0);
                search_list_select(priv->search_list, attr_town_or_district_name, id, 1);
            } else if (type==3){ // 3 = street in android
                search_list_select(priv->search_list, attr_street_name, 0, 0);
                search_list_select(priv->search_list, attr_street_name, id, 1);
            }
        } else if (type){
            start_search(priv, search_string, type);// 2 = search for town, 3 = street
        }
    } else {
        dbg(lvl_error, "Error: no priv (handle), failed");
    }

    (*env)->ReleaseStringUTFChars(env, str_, search_string);
}


JNIEXPORT jstring JNICALL
Java_org_navitproject_navit_NavitAppConfig_callbackLocalizedString(JNIEnv *env, jclass type, jstring s_)
{
  const char *s;
  const char *localized_str;

  s=(*env)->GetStringUTFChars(env, s_, NULL);
  localized_str=navit_nls_gettext(s);
  jstring js = (*env)->NewStringUTF(env,localized_str);
  (*env)->ReleaseStringUTFChars(env, s_, s);

  return js;
}

