/* RealtimeAlertsList.c - manage the Real Time Alerts list display
 *
 * LICENSE:
 *
 *   Copyright 2008 Ehud Shabtai
 *
 *   This file is part of RoadMap.
 *
 *   RoadMap is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License V2 as published by
 *   the Free Software Foundation.
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
 *   See RealtimeAlertsList.h
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_history.h"
#include "roadmap_locator.h"
#include "roadmap_street.h"
#include "roadmap_lang.h"
#include "roadmap_messagebox.h"
#include "roadmap_geocode.h"
#include "roadmap_trip.h"
#include "roadmap_county.h"
#include "roadmap_display.h"
#include "roadmap_math.h"
#include "roadmap_navigate.h"
#include "ssd/ssd_keyboard.h"
#include "ssd/ssd_generic_list_dialog.h"
#include "ssd/ssd_confirm_dialog.h"
#include "ssd/ssd_tabcontrol.h"
#include "ssd/ssd_list.h"
#include "ssd/ssd_contextmenu.h"
#include "ssd/ssd_dialog.h"

#include "RealtimeAlerts.h"
#include "RealtimeAlertsList.h"
#include "RealtimeAlertCommentsList.h"


// Context menu:
static ssd_cm_item      context_menu_items[] =
{
   SSD_CM_INIT_ITEM  ( "Show on map",        rtl_cm_show),
   SSD_CM_INIT_ITEM  ( "Delete",             rtl_cm_delete),
   SSD_CM_INIT_ITEM  ( "View comments",      rtl_cm_view_comments),
   SSD_CM_INIT_ITEM  ( "Post Comment",       rtl_cm_add_comments),
   SSD_CM_INIT_ITEM  ( "Sort by proximity",  rtl_cm_sort_proximity),
   SSD_CM_INIT_ITEM  ( "Sort by recency",    rtl_cm_sort_recency),
   SSD_CM_INIT_ITEM  ( "Exit_key",           rtl_cm_exit),
   SSD_CM_INIT_ITEM  ( "Cancel",             rtl_cm_cancel),
};
static ssd_contextmenu  context_menu = SSD_CM_INIT_MENU( context_menu_items);

static void populate_list();
static void set_softkeys( SsdWidget dialog);

static SsdWidget   tabs[tab__count];
static AlertList gAlertListTable;
static AlertList gTabAlertList;

static   BOOL                               g_context_menu_is_active= FALSE;
static   real_time_tabs     g_active_tab = tab_all;
static   alert_sort_method g_list_sorted = sort_proximity;

///////////////////////////////////////////////////////////////////////////////////////////
static int RealtimeAlertsListCallBack(SsdWidget widget, const char *new_value,
        const void *value, void *context)
{

    int alertId;

    if (!new_value[0] || !strcmp(new_value,
            roadmap_lang_get("Scroll Through Alerts on the map")))
    {
        RTAlerts_Scroll_All();
        ssd_generic_list_dialog_hide_all();
        return TRUE;
    }

    alertId = atoi((const char*)value);
    if (RTAlerts_Get_Number_of_Comments(alertId) == 0)
    {
        ssd_generic_list_dialog_hide_all();
        RTAlerts_Popup_By_Id(alertId);
    }
    else
    {
        ssd_generic_list_dialog_hide();
        RealtimeAlertCommentsList(alertId);
    }
    return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////
static void delete_callback(int exit_code, void *context){
    int alertId;
    BOOL success;

    if (exit_code != dec_yes)
         return;

    alertId = atoi((const char*)context);
    success = real_time_remove_alert(alertId);
    if (success)
        roadmap_messagebox("Delete Alert", "Your request was sent to the server");

}

///////////////////////////////////////////////////////////////////////////////////////////
static int RealtimeAlertsDeleteCallBack(SsdWidget widget, const char *new_value,
        const void *value, void *context)
{
   char message[200];


    if (!new_value[0] || !strcmp(new_value,
            roadmap_lang_get("Scroll Through Alerts on the map")))
    {
         return TRUE;
    }

     sprintf(message,"%s\n%s",roadmap_lang_get("Delete Alert:"), new_value);

    ssd_confirm_dialog("Delete Alert", message,FALSE, delete_callback,  (void *)value);

    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////
void RealtimeAlertsListNoTabs(void)
{
    populate_list();
    ssd_generic_icon_list_dialog_show(roadmap_lang_get(ALERTS_LIST_TITLE), gAlertListTable.iCount,
            (const char **)&gAlertListTable.labels[0], (const void **)&gAlertListTable.values[0], (const char **)&gAlertListTable.icons[0],NULL,
            RealtimeAlertsListCallBack, RealtimeAlertsDeleteCallBack, NULL,
            NULL,
            NULL,
            80,
            0,
            TRUE);
}

///////////////////////////////////////////////////////////////////////////////////////////
static int RealtimeAlertsListCallBackTabs(SsdWidget widget, const char *new_value,
        const void *value)
{

    int alertId;

    if (!new_value[0] || !strcmp(new_value,
            roadmap_lang_get("Scroll Through Alerts on the map")))
    {
        RTAlerts_Scroll_All();
        ssd_generic_list_dialog_hide_all();
        return TRUE;
    }

    alertId = atoi((const char*)value);
    if (RTAlerts_Get_Number_of_Comments(alertId) == 0)
    {
        ssd_generic_list_dialog_hide_all();
        RTAlerts_Popup_By_Id(alertId);
    }
    else
    {
        ssd_generic_list_dialog_hide();
        RealtimeAlertCommentsList(alertId);
    }
    return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////
static int RealtimeAlertsDeleteCallBackTabs(SsdWidget widget, const char *new_value,
        const void *value)
{
   char message[200];


    if (!new_value[0] || !strcmp(new_value,
            roadmap_lang_get("Scroll Through Alerts on the map")))
    {
         return TRUE;
    }

     sprintf(message,"%s\n%s",roadmap_lang_get("Delete Alert:"), new_value);

    ssd_confirm_dialog("Delete Alert", message,FALSE, delete_callback,  (void *)value);

    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////
static void populate_tab(int tab, int num, int types,...){
    int i;
    int count = 0;
    va_list ap;
    int type;

    for ( i=0; i< gAlertListTable.iCount; i++){
        int j;
        va_start(ap, types);
        type = types;
        for (j = 0; j < num; j++ ) {
            if (gAlertListTable.type[i] == type) {
                gTabAlertList.labels[count] = gAlertListTable.labels[i];
                gTabAlertList.values[count] = gAlertListTable.values[i];
                gTabAlertList.icons[count] = gAlertListTable.icons[i];
                count++;
            }
            type = va_arg(ap, int);
        }
        va_end(ap);
     }
     gTabAlertList.iCount = count;
     if (count > 0)
         ssd_list_populate (tabs[tab], count, (const char **)&gTabAlertList.labels[0], (const void **)&gTabAlertList.values[0],(const char **)&gTabAlertList.icons[0], NULL,RealtimeAlertsListCallBackTabs, RealtimeAlertsDeleteCallBackTabs, TRUE);

}

///////////////////////////////////////////////////////////////////////////////////////////
static void populate_list(){
    int iCount = -1;
    int iAlertCount;
    RTAlert *alert;
    char AlertStr[300];
    char str[100];
    int distance = -1;
    RoadMapGpsPosition CurrentPosition;
    RoadMapPosition position, current_pos;
    const RoadMapPosition *gps_pos;
    PluginLine line;
    int Direction;
    int distance_far;
    char dist_str[100];
    char unit_str[20];
    int i;
    RoadMapPosition context_save_pos;
    int context_save_zoom;

    AlertStr[0] = 0;
    str[0] = 0;

    for (i=0; i<MAX_ALERTS_ENTRIES; i++)
    {
        gAlertListTable.icons[i] = NULL;
    }

    if (iCount == -1)
    {
        gAlertListTable.labels[0]
                = (char *)roadmap_lang_get("Scroll Through Alerts on the map");
        gAlertListTable.icons[0] = "globe";
        gAlertListTable.type[0] = -1;

    }
    iCount = 1;

    iAlertCount = RTAlerts_Count();

     roadmap_math_get_context(&context_save_pos, &context_save_zoom);

    while ((iAlertCount > 0) && (iCount < MAX_ALERTS_ENTRIES))    {

        AlertStr[0] = 0;

        alert = RTAlerts_Get(iCount -1);

        position.longitude = alert->iLongitude;
        position.latitude = alert->iLatitude;

        roadmap_math_set_context((RoadMapPosition *)&position, 20);

        if (alert->iType == RT_ALERT_TYPE_ACCIDENT)
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                    - strlen(AlertStr), "%s", roadmap_lang_get("Accident"));
        else if (alert->iType == RT_ALERT_TYPE_POLICE)
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                    - strlen(AlertStr), "%s", roadmap_lang_get("Police"));
        else if (alert->iType == RT_ALERT_TYPE_TRAFFIC_JAM)
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                    - strlen(AlertStr), "%s", roadmap_lang_get("Traffic jam"));
        else if (alert->iType == RT_ALERT_TYPE_TRAFFIC_INFO)
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                 - strlen(AlertStr), "%s", roadmap_lang_get("Road info"));
        else if (alert->iType == RT_ALERT_TYPE_HAZARD)
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                 - strlen(AlertStr), "%s", roadmap_lang_get("Hazard"));
        else if (alert->iType == RT_ALERT_TYPE_OTHER)
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                 - strlen(AlertStr), "%s", roadmap_lang_get("Other"));
        else if (alert->iType == RT_ALERT_TYPE_CONSTRUCTION)
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                 - strlen(AlertStr), "%s", roadmap_lang_get("Road construction"));
        else
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                    - strlen(AlertStr), "%s",
                    roadmap_lang_get("Chit Chat"));

        if (roadmap_navigate_get_current(&CurrentPosition, &line, &Direction)
                == -1)
        {
            // check the distance to the alert
            gps_pos = roadmap_trip_get_position("GPS");
            if (gps_pos != NULL)
            {
                current_pos.latitude = gps_pos->latitude;
                current_pos.longitude = gps_pos->longitude;
            }
            else
            {
                current_pos.latitude = -1;
                current_pos.longitude = -1;
            }
        }
        else
        {
            current_pos.latitude = CurrentPosition.latitude;
            current_pos.longitude = CurrentPosition.longitude;
        }

        if ((current_pos.latitude != -1) && (current_pos.longitude != -1))
        {
            distance = roadmap_math_distance(&current_pos, &position);
            distance_far = roadmap_math_to_trip_distance(distance);

            if (distance_far > 0)
            {
                int tenths = roadmap_math_to_trip_distance_tenths(distance);
                snprintf(dist_str, sizeof(str), "%d.%d", distance_far, tenths
                        % 10);
                snprintf(unit_str, sizeof(unit_str), "%s",
                        roadmap_lang_get(roadmap_math_trip_unit()));
            }
            else
            {
                snprintf(dist_str, sizeof(str), "%d", distance);
                snprintf(unit_str, sizeof(unit_str), "%s",
                        roadmap_lang_get(roadmap_math_distance_unit()));
            }
            snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                    - strlen(AlertStr), " (%s %s)", dist_str, unit_str);
        }

        snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                - strlen(AlertStr), "\n%s", alert->sLocationStr);

        snprintf(AlertStr + strlen(AlertStr), sizeof(AlertStr)
                - strlen(AlertStr), "\n%s", alert->sDescription);

        snprintf(str, sizeof(str), "%d", alert->iID);

        //      if (labels[iCount]) free (labels[iCount]);
        gAlertListTable.labels[iCount] = strdup(AlertStr);

        gAlertListTable.values[iCount] = strdup(str);
        gAlertListTable.icons[iCount] = strdup(RTAlerts_Get_Icon(alert->iID));
        gAlertListTable.type[iCount] = alert->iType;
        gAlertListTable.iDistnace[iCount] = distance;
        iCount++;
        iAlertCount--;
    }

     gAlertListTable.iCount = iCount;

    roadmap_math_set_context(&context_save_pos, context_save_zoom);
}

///////////////////////////////////////////////////////////////////////////////////////////
static SsdWidget create_list(int tab_id){
    SsdWidget list;
    char tab_name[20];
    sprintf(tab_name, "list %d", tab_id);
    list = ssd_list_new (tab_name, SSD_MAX_SIZE, SSD_MAX_SIZE, inputtype_none, 0, NULL);
#ifdef IPHONE
    ssd_list_resize (list, 73);
#elif defined (TOUCH_SCREEN)
    ssd_list_resize (list, 66);
#else
    ssd_list_resize (list, 63);
#endif

    return list;
}

///////////////////////////////////////////////////////////////////////////////////////////
static void populate_first_tab(){
    if (gAlertListTable.iCount > 0)
        ssd_list_populate (tabs[0], gAlertListTable.iCount, (const char **)&gAlertListTable.labels[0], (const void **)&gAlertListTable.values[0],(const char **)&gAlertListTable.icons[0], NULL,RealtimeAlertsListCallBackTabs, RealtimeAlertsDeleteCallBackTabs, TRUE);
}

///////////////////////////////////////////////////////////////////////////////////////////
static void on_tab_gain_focus(int tab)
{
    switch (tab){
        case tab_all:
            populate_first_tab();
            break;
        case tab_police:
            populate_tab(tab_police,1,RT_ALERT_TYPE_POLICE);
            break;
        case tab_traffic_jam:
            populate_tab(tab_traffic_jam,2, RT_ALERT_TYPE_TRAFFIC_INFO, RT_ALERT_TYPE_TRAFFIC_JAM);
            break;
        case tab_accidents:
            populate_tab(tab_accidents,1, RT_ALERT_TYPE_ACCIDENT);
            break;
        case tab_chit_chat:
            populate_tab(tab_chit_chat,4, RT_ALERT_TYPE_CHIT_CHAT, RT_ALERT_TYPE_HAZARD,RT_ALERT_TYPE_OTHER, RT_ALERT_TYPE_CONSTRUCTION);
            break;

            default:
                break;
    }
    g_active_tab = tab;
}

///////////////////////////////////////////////////////////////////////////////////////////
static void clear_lists(){
    int i;
      for (i=0; i< tab__count;i++){
        ssd_list_populate (tabs[i], 0, NULL, NULL,NULL, NULL,RealtimeAlertsListCallBackTabs, RealtimeAlertsDeleteCallBackTabs, TRUE);
      }
}

///////////////////////////////////////////////////////////////////////////////////////////
static SsdTcCtx create_tab_control(){
    SsdTcCtx    tabcontrol;
   const char* tabs_titles[tab__count];
    int i;

   tabs_titles[tab_all] = roadmap_lang_get("All");
   tabs_titles[tab_police] = roadmap_lang_get("Police");
   tabs_titles[tab_traffic_jam] = roadmap_lang_get("Traffic Jams");
   tabs_titles[tab_accidents] = roadmap_lang_get("Accidents");
   tabs_titles[tab_chit_chat] = roadmap_lang_get("Chit Chats");

   for (i=0; i< tab__count;i++)
       tabs[i] = create_list(i);



    RTAlerts_Sort_List(sort_proximity);

    RTAlerts_Stop_Scrolling();

    populate_list();

   tabcontrol = ssd_tabcontrol_new(
                              ALERTS_LIST_TITLE,
                              roadmap_lang_get(ALERTS_LIST_TITLE),
                              NULL,
                              NULL,
                              on_tab_gain_focus,
                              NULL,
                              tabs_titles,
                              tabs,
                              tab__count,
                              0);

    for (i=0; i< tab__count;i++){
        ssd_dialog_set_drag(ssd_tabcontrol_get_dialog(tabcontrol),tabs[i]);
    }

    set_softkeys( ssd_tabcontrol_get_dialog(tabcontrol));

    populate_first_tab();

    return tabcontrol;
}


///////////////////////////////////////////////////////////////////////////////////////////
static SsdTcCtx  get_tabcontrol()
{
   static SsdTcCtx  tabcontrol = NULL;

   if( tabcontrol){
    g_list_sorted =sort_proximity;
    RTAlerts_Sort_List(sort_proximity);
    RTAlerts_Stop_Scrolling();
    clear_lists();
    populate_list();
    populate_first_tab();
    ssd_tabcontrol_set_active_tab(tabcontrol, tab_all);
    return tabcontrol;
   }
   else{
       tabcontrol =  create_tab_control();
       return tabcontrol;
   }
}

///////////////////////////////////////////////////////////////////////////////////////////
void RealtimeAlertsList(){

//#ifdef TOUCH_SCREEN
//  RealtimeAlertsListNoTabs();
//#else
   SsdTcCtx tabcontrol =  get_tabcontrol();
    ssd_tabcontrol_show(tabcontrol);
//#endif
}

///////////////////////////////////////////////////////////////////////////////////////////
static void on_option_delete(){
    char message[300];
    SsdWidget list = tabs[g_active_tab];
    const void *value = ssd_list_selected_value ( list);
    const char* string   = ssd_list_selected_string( list);

    if (value != NULL){

         sprintf(message,"%s\n%s",roadmap_lang_get("Delete Alert:"),(const char *)string);

         ssd_confirm_dialog("Delete Alert", message,FALSE, delete_callback,  (void *)value);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
static void on_option_show(){
        SsdWidget list = tabs[g_active_tab];
        const void *value = ssd_list_selected_value ( list);
        if (value != NULL){
                int alertId = atoi((const char*)value);
               ssd_generic_list_dialog_hide_all();
              RTAlerts_Popup_By_Id(alertId);
     }

}

///////////////////////////////////////////////////////////////////////////////////////////
static void on_option_show_comments(){
        SsdWidget list = tabs[g_active_tab];
        const void *value = ssd_list_selected_value ( list);
        if (value != NULL){
                int alertId = atoi((const char*)value);
                if (RTAlerts_Get_Number_of_Comments(alertId) != 0)
                {
                    ssd_generic_list_dialog_hide();
                      RealtimeAlertCommentsList(alertId);
                }
     }
}

///////////////////////////////////////////////////////////////////////////////////////////
static void on_option_add_comment(){
        SsdWidget list = tabs[g_active_tab];
        const void *value = ssd_list_selected_value ( list);
        if (value != NULL){
                int alertId = atoi((const char*)value);
                real_time_post_alert_comment_by_id(alertId);
        }
}

///////////////////////////////////////////////////////////////////////////////////////////
static void on_option_sort_proximity(){
    g_list_sorted =sort_proximity;
    RTAlerts_Sort_List(sort_proximity);
    clear_lists();
    populate_list();
    on_tab_gain_focus(g_active_tab);
}

///////////////////////////////////////////////////////////////////////////////////////////
static void on_option_sort_recency(){
    g_list_sorted =sort_recency;
    RTAlerts_Sort_List(sort_recency);
    clear_lists();
    populate_list();
    on_tab_gain_focus(g_active_tab);
}

///////////////////////////////////////////////////////////////////////////////////////////
static void on_option_selected(  BOOL              made_selection,
                                 ssd_cm_item_ptr   item,
                                 void*             context)
{
   real_time_list_context_menu_items   selection;
   int                                 exit_code = dec_ok;

   g_context_menu_is_active = FALSE;

   if( !made_selection)
      return;

   selection = (real_time_list_context_menu_items)item->id;

   switch( selection)
   {
      case rtl_cm_show:
        on_option_show();
         break;

      case rtl_cm_delete:
        on_option_delete();
         break;

      case rtl_cm_view_comments:
        on_option_show_comments();
         break;

      case rtl_cm_add_comments:
         on_option_add_comment();
         break;

      case rtl_cm_sort_proximity:
      on_option_sort_proximity();
         break;

      case rtl_cm_sort_recency:
        on_option_sort_recency();
         break;


      case rtl_cm_exit:
         exit_code = dec_cancel;
        ssd_dialog_hide_all( exit_code);
        roadmap_screen_refresh ();
         break;
      case rtl_cm_cancel:
        g_context_menu_is_active = FALSE;
        roadmap_screen_refresh ();
        break;

      default:
         break;
   }
}

///////////////////////////////////////////////////////////////////////////////////////////

static int on_options(SsdWidget widget, const char *new_value, void *context)
{
    int menu_x;
    BOOL has_comments = FALSE;
    BOOL is_empty = FALSE;
    BOOL is_selected = TRUE;
    SsdWidget list ;
    const void *value;
    BOOL add_cancel = FALSE;
    BOOL add_exit = TRUE;


#ifdef TOUCH_SCREEN
   add_cancel = TRUE;
   add_exit = FALSE;
   roadmap_screen_refresh();
#endif

   is_empty = ((g_active_tab == 0) && (gAlertListTable.iCount < 2)) || ((g_active_tab != 0) && (gTabAlertList.iCount < 1));
   if(g_context_menu_is_active)
   {
      ssd_dialog_hide_current(dec_ok);
      g_context_menu_is_active = FALSE;
      return 0;
   }

    list = tabs[g_active_tab];
    value = ssd_list_selected_value ( list);
    if (value != NULL){
        int alertId = atoi((const char*)value);
        if (RTAlerts_Get_Number_of_Comments(alertId) != 0)
            has_comments = TRUE;

    }
    else
        is_selected = FALSE;

   ssd_contextmenu_show_item( &context_menu,
                              rtl_cm_show,
                              !is_empty && is_selected,
                              FALSE);
   ssd_contextmenu_show_item( &context_menu,
                              rtl_cm_add_comments,
                              !is_empty && is_selected,
                              FALSE);


   ssd_contextmenu_show_item( &context_menu,
                              rtl_cm_delete,
                              !is_empty && is_selected,
                              FALSE);
   ssd_contextmenu_show_item( &context_menu,
                              rtl_cm_view_comments,
                              !is_empty && is_selected && has_comments,
                              FALSE);

   ssd_contextmenu_show_item( &context_menu,
                              rtl_cm_sort_proximity,
                              (g_list_sorted == sort_recency) && !is_empty,
                              FALSE);
   ssd_contextmenu_show_item( &context_menu,
                              rtl_cm_sort_recency,
                              (g_list_sorted == sort_proximity && !is_empty),
                              FALSE);

   ssd_contextmenu_show_item( &context_menu,
                              rtl_cm_exit,
                              add_exit,
                              FALSE);

   ssd_contextmenu_show_item( &context_menu,
                              rtl_cm_cancel,
                              add_cancel,
                              FALSE);

   if  (ssd_widget_rtl (NULL))
       menu_x = SSD_X_SCREEN_RIGHT;
    else
        menu_x = SSD_X_SCREEN_LEFT;

   ssd_context_menu_show(  menu_x,              // X
                           SSD_Y_SCREEN_BOTTOM, // Y
                           &context_menu,
                           on_option_selected,
                           NULL,
                           dir_default);

   g_context_menu_is_active = TRUE;

   return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
static void set_softkeys( SsdWidget dialog)
{
   ssd_widget_set_left_softkey_text       ( dialog, roadmap_lang_get("Options"));
   ssd_widget_set_left_softkey_callback   ( dialog, on_options);
}
