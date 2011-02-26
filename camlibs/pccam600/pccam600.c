/****************************************************************/
/* pccam600.c - Gphoto2 library for the Creative PC-CAM600      */
/*                                                              */
/*                                                              */
/* Author: Peter Kajberg <pbk@odense.kollegienet.dk>            */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* Please notice that camera commands was sniffed by use of a   */
/* usbsniffer under windows. This is an experimental driver and */
/* a work in progess(I hope :))                                 */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 59 Temple Place - Suite 330, */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#include "config.h"
#include <string.h>


#include "pccam600.h"

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-log.h>

#define GP_MODULE "pccam600"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

/*
 *waits until the status value is 0 or 8. 
 *if status == 0xb0 or 0x40 we will wait some more
 */
static void pccam600_wait_for_status(GPPort *port){
  int ret;
  unsigned char status = 1;
  while(status != 0x00){
    gp_port_set_timeout(port,3000);
    ret = gp_port_usb_msg_read(port,0x06,0x00,0x00,&status,1);
    if (status == 0 || status == 8)
      return;
    if (status == 0xb0){
      gp_port_set_timeout(port,200000);
      ret = gp_port_usb_msg_read(port,0x06,0x00,0x00,&status,1);
    }
    if (status == 0x40){
      gp_port_set_timeout(port,400000);
      ret = gp_port_usb_msg_read(port,0x06,0x00,0x00,&status,1);
    }
  }
}

/*
 *Deletes a file. The first file has index = 2.
 */
int pccam600_delete_file(GPPort *port, GPContext *context, int index){
  unsigned char response[4];
  int ret;
  index = index + 2;
  gp_port_set_timeout(port,200000);
  ret = gp_port_usb_msg_write(port,0x09,index,0x1001,NULL,0x00);
  pccam600_wait_for_status(port);
  gp_port_set_timeout(port, 400000);
  ret = gp_port_usb_msg_read(port,0x60,0x00,0x03,response,0x04);
  pccam600_wait_for_status(port);
  ret = gp_port_usb_msg_read(port,0x60,0x00,0x04,response,0x04);
  pccam600_wait_for_status(port);
  return GP_OK;
}

int pccam600_get_mem_info(GPPort *port, GPContext *context, int *totalmem, 
			    int *freemem)
{
  unsigned char response[4];
  int ret;
  gp_port_set_timeout(port, 400000);
  ret = gp_port_usb_msg_read(port,0x60,0x00,0x03,response,0x04);  
  *totalmem = response[2]*65536+response[1]*256+response[0];
  pccam600_wait_for_status(port);
  ret = gp_port_usb_msg_read(port,0x60,0x00,0x04,response,0x04);
  *freemem = response[2]*65536+response[1]*256+response[0];
  pccam600_wait_for_status(port);
  return GP_OK;
}

/*
 *
 */
int pccam600_get_file_list(GPPort *port, GPContext *context){
  unsigned char response[4];
  int ret, nr_of_blocks;
  gp_port_set_timeout(port,500);
  ret = gp_port_usb_msg_write(port,0x08,0x00,0x1021,NULL,0x00);
  pccam600_wait_for_status(port);
  gp_port_set_timeout(port, 200000);
  ret = gp_port_usb_msg_write(port,0x08,0x00,0x1021,NULL,0x00);
  pccam600_wait_for_status(port);
  ret = gp_port_usb_msg_read(port,0x08,0x00,0x1000,response,0x04);
  nr_of_blocks = response[2]*256+response[1];
  if (nr_of_blocks == 0){
    gp_log(GP_LOG_DEBUG,"pccam600 library: pccam600_get_file_list",
	   "nr_of_blocks is 0");
    gp_context_error(context,_("pccam600_init:Expected > %d blocks got %d"),
		 0,nr_of_blocks); 
    return GP_ERROR;
  }
  return nr_of_blocks / 2;
}

int pccam600_get_file(GPPort *port, GPContext *context, int index){
  unsigned char response[4];
  int ret,nr_of_blocks;
  index = index + 2;
  if (index < 2)  {
    gp_context_error(context,
		 _("pccam600_get_file:got index %d but expected index > %d"),
		 index,2);
    return GP_ERROR;
  }
  gp_port_set_timeout(port,200000);
  ret = gp_port_usb_msg_read(port,0x08,index,0x1001,response,0x04);
  gp_port_set_timeout(port,3000);
  ret = gp_port_usb_msg_write(port,0x04,0x00,0x00,NULL,0x00);
  pccam600_wait_for_status(port);
  gp_port_set_timeout(port,200000);
  ret = gp_port_usb_msg_read(port,0x08,index,0x1002,response,0x04);
  ret = gp_port_usb_msg_read(port,0x08,index,0x1001,response,0x04);
  nr_of_blocks = response[2]*256+response[1];
  if (nr_of_blocks == 0){
    gp_log(GP_LOG_DEBUG,
	   "pccam600 library: pccam600_get_file","nr_of_msg is 0");
    gp_context_error(context,_("pccam600_init:Expected > %d blocks got %d"),
		 0,nr_of_blocks); 
    return GP_ERROR;
  }
  return nr_of_blocks / 2;
}

/*
 *Reads bulk data from the camera in 512bytes chunks.
 */
int pccam600_read_data(GPPort *port, unsigned char *buffer){
  int ret;
  gp_port_set_timeout(port,500);
  ret = gp_port_read(port,buffer,512);
  return GP_OK;
}

/*
 *
 */
int pccam600_close(GPPort *port, GPContext *context){
  int ret;
  gp_port_set_timeout(port,500);
  ret = gp_port_usb_msg_write(port,0x08,0x00,0xf0,NULL,0x00);
  if (ret < 0) {
    gp_context_error(context,
		 _("pccam600_close:return value was %d instead of %d"),
		 ret,0);
    return GP_ERROR;
  }
  pccam600_wait_for_status(port);
  return GP_OK;
}

/*
 *Sets up the camera and then read 32 512bytes blocks.
 *Why it is read those 32 blocks I dont know. Just
 *doing what the windows driver does :).
 */
int pccam600_init(GPPort *port, GPContext *context){
  unsigned char response[4];
  unsigned char buffer[512];
  int nr_of_blocks;
  int ret,i;
  gp_port_set_timeout(port,100);
  ret = gp_port_usb_msg_write(port,0x0e,0x00,0x01,NULL,0x0);
  ret = gp_port_usb_msg_write(port,0x08,0x00,0xff,NULL,0x0);
  pccam600_wait_for_status(port);
  if (ret <0) return ret;
  gp_port_set_timeout(port,100000);
  ret = gp_port_usb_msg_read(port,0x08,0x00,0xff,response,0x1);
  gp_port_set_timeout(port,500);
  ret = gp_port_usb_msg_write(port,0x08,0x00,0x1020,NULL,0x0);
  pccam600_wait_for_status(port);
  gp_port_set_timeout(port,200000);
  ret = gp_port_usb_msg_read(port,0x08,0x00,0x1000,response,0x4);
  nr_of_blocks = response[2]*256+response[1];
  nr_of_blocks = 512 / nr_of_blocks;
  gp_log(GP_LOG_DEBUG,"pccam600 library: init","nr_of_blocks %d",nr_of_blocks);
  if (nr_of_blocks == 0){
    gp_context_error(context,_("pccam600_init:Expected %d blocks got %d"),
		 64,nr_of_blocks); 
    return GP_ERROR;
  }
  gp_port_set_timeout(port,500);
  for (i = 0; i<nr_of_blocks; i++){
    ret = gp_port_read(port, buffer,512);
    if (ret < 0){
      gp_log(GP_LOG_DEBUG,
	     "pccam600 library: init"," gp_port_read returned %d:",
	     ret);
      gp_context_error(context,_("pccam600 init:"
		   "Unexpected error: gp_port_read returned %d instead of %d"),
		   ret,0);
      return GP_ERROR;
    }
  }
  return GP_OK;
  
}
