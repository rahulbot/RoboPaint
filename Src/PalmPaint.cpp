/******************************************************************************
 * PalmPaint - a little PalmOS color painting program
 *
 * Copyright (C) 2001 Rahul Bhargava
 * <rahulb@yahoo.com>
 * <http://www.geocities.com/rahulb/>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 *****************************************************************************/

/******************************************************************************
 *
 * 							   PalmPaint v1.0
 *						---------------------------
 *							   Rahul Bhargava 
 *							 <rahulb@yahoo.com>
 *					 <http://www.geocities.com/rahulb>
 *
 *  INTRO
 *  -----
 * 
 *  This is my kick-ass drawing application.  Right now it is pretty nice, lots
 *	things I want to add, but you gotta release it sometime!  Decision to only
 *	support color PalmOS devices was intentional - great drawing programs for
 *	regular PalmOS already exist.  I had all the features already made, so I
 *	spent a weekend cleaning up the interface and here it is.
 *
 *	This was developed using CodeWarrior for PalmOS 6 (Mac).  I've tried to
 *	comment pretty extensively because I'm very good at forgetting what code
 *	I've written actually does.  Hopefully the time spent doing this will be
 *	useful at some point!
 *
 *	Thanks to Casey and Mr. Derfner for all their help on this!
 *
 *	POSTCARD-WARE
 *	-------------
 *
 *	In homage to the old days of BBSing (and thanks to my friend Dan's suggestion),
 *	I've released PalmPaint as "postcard-ware". That means that if you use PalmPaint
 *	and like it, all I ask is that you send me a postcard from where you live.  Come
 *	on - postcards are cheap, and I love getting mail!  My address is:
 *
 *				Rahul Bhargava
 *				Bldg. E15-020d
 *				20 Ames St.
 *				Cambridge, MA 02139
 *
 *  REVISION HISTORY
 *  ----------------
 *
 *  2000.01.30 : v1.0 : Initial release to the public (under GNU GPL)	
 *		- A saveable swatch of colors (double-tap to edit)
 *		- Erase (double-tap to erase full screen)
 *		- Copy and paste (option of white being transparent)
 *		- Undo (one-level)
 *		- Eye-dropper to select colors off the screen
 *		- Resizeable pen (click on up and down arrows), always a circle
 *		- File management (open, save, and delete)
 *		- Wicked cool interface (with graffitti shortcuts for all menu commands)
 *		- Tons of commenting (it's open-source, so I felt the need)
 *		- Lots of debugging code left in and just turned off
 *
 *	To Do: at some point...
 *		- use clipboard copy and paste function instead of a BitmapPtr/BitmapWindow
 *		- ability to name images (implement stringDB)
 *		- animation control? how would I interrupt?
 *		- bigger image size (and thus a scrolling image...)
 *		- conduit to transfer images back and forth to MAC/PC
 *
 *****************************************************************************/

#include <PalmOS.h>
#include "PalmPaintRsc.h"

/***********************************************************************
 *   Internal Constants
 ***********************************************************************/

#define DB_TYPE						'DATA'		//type of the databases I create
#define DB_NAME_BMP					"BitmapDB:pmPt"
#define DB_NAME_STR					"StringDB:pmPt"	//not currently implemented 

//registered pmPT with Palm at http://www.palmos.com/dev/tech/palmos/creatorid/
#define appFileCreator				'pmPT'		//unique creatorID

#define appPrefID					0x00		//do I need this
#define appPrefVersionNum			0x01		//or this?

// Define the minimum OS version we support (2.0 for now).
#define ourMinVersion	sysMakeROMVersion(3,5,0,sysROMStageRelease,0)

//min and max macros because I need a Math library!
#define MIN(a,b)	((a>b) ? b : a)
#define MAX(a,b)	((a>b) ? a : b)

//Use these to limit pen resizing - my fucked up drawing algorithm really slows
//  down as the size increases beyond 15 or so
#define PEN_SIZE_MAX 				15
#define PEN_SIZE_MIN				1

//these are mainly used in the main event loop to determine what to do
// with a pen event, they basically determine the "state" of the program
#define MODE_DRAW					0		//regular drawing mode
#define MODE_COPY					2		//draw a selection rectangle till a penUp
#define MODE_PASTE					3		//paste on a penUp
#define MODE_ANIMATE				4		//for animation stuff, not currently used
#define MODE_CONTROL				5		//for recognizing gesture, not currently used
#define MODE_GET_COLOR				6		//the eye dropper, grab pixel color on penUp
#define MODE_PICK_COLOR				7		//the color selection form is open

/***********************************************************************
 *   Entry Points
 *
 *		What the hell is supposed to go here?
 ***********************************************************************/

/***********************************************************************
 *   Internal Structures
 *
 *		Right now these are spare.  I should probably make a
 *		BufferBitmapType with a bitmapP and a WinHandle in it, to then
 *		instantiate twice for Copy and Buffer.
 ***********************************************************************/

typedef struct 
	{
	//these should probably be in an array, but I can't get that to work
	// with the saving and loading of preferences, probably because an
	// array is a pointer and thus dynamically allocated
	RGBColorType clr0;		//save the foreground color here
	RGBColorType clr1;		//save the background color here - always white currently
	RGBColorType clr2;
	RGBColorType clr3;
	RGBColorType clr4;
	RGBColorType clr5;
	RGBColorType clr6;
	} PPPrefType;			//PalmPaint preferences type

//I just find these useful
typedef Char* CharPtr;
typedef RGBColorType* RGBColorPtr;

/***********************************************************************
 *   Global variables
 *
 *		These are quite useful.
 ***********************************************************************/

//color management variables
RGBColorPtr colors  =  new RGBColorType[7];		//holds the user's current swatch, make this saveable, [0] is current fore color
int currColorIndex=0;							//which color is the current one, 0 if not in swatch
RGBColorPtr clrCopyRectP = new RGBColorType;	//the color of the rectangle in copy mode

//draw management variables
RectanglePtr drawAreaRctP = new RectangleType;	//the main drawing area on the screen
RectanglePtr selectionRctP = new RectangleType;	//use this for selecting drawings
int oldX,oldY;									//for tracking pen movements
int mode = MODE_DRAW;							//current state of the interaction
int PenSize = 6;								//this holds the current pen size

//copy-paste functionality variables
BitmapPtr copyBmpP;								//a bitmap of what the user has copied
WinHandle copyWinH;								//need this to hold the copy bmp
BitmapPtr bufferBmpP;							//a bitmap of the drawing area, for redrawing
WinHandle bufferWinH;							//need this to hold the buffer bmp
Boolean pasteTransparent=false;					//if true then paste white as transparent

//animation variables
int tpf;										//the number of ticks per frame
PointType *bmpPtP;								//the current location of the bitmap
RectanglePtr animAreaRctP = new RectangleType;	//the main animation area on the screen

//random
FieldPtr outputFldP;							//for debugging use this field
PPPrefType p;									//my preferences, for saving color swatch
int DblClickDelay;								//ticks speed to qualify as a double-tap
Boolean DEBUG = false;							//if true then print outputs

//Database variables
DmOpenRef myBmpDB;								//my database of images
DmOpenRef myStrDB;								//my database of the image titles ?
int currImageID = -1;							//the db id of the image opened
int deleteImageID = -1;							//the db id of the image to be deleted
int stringDB = false;							//if true, then creates and uses stringDB

/***********************************************************************
 *   Internal Function Prototypes
 *
 *		This is just how you do it in C++.
 ***********************************************************************/

//drawing functons
void DR_Copy(RectanglePtr rP);
void DR_BufferScreen();
void DR_DrawBuffer();
void DR_Paste(PointType* tl);

//utility functions
void UTIL_CleanUpMemory();
void UTIL_Output(Char* str);
Char *UTIL_itos(int n);
int UTIL_GetScreenMode();

//animation functions
void AN_MoveToEdge(PointType *startPtP,int xStep,int yStep);

//color functions
void CLR_CopyToFore(int s);
void CLR_CopyToBack(int s);
void CLR_DrawSwatch();
void CLR_FillInEyeDropper(RGBColorPtr clr);
void CLR_FillInEraser(RGBColorPtr clr);
void PickColorFormInit();

//database functions
BitmapPtr DM_LoadBitmap(DmOpenRef db, int index);
void DM_SaveBitmap(DmOpenRef db,unsigned short index, BitmapPtr myBmpP);
void DM_DeleteBitmap(DmOpenRef db,int index);
Char** DM_ListSavedImages(DmOpenRef db);

//open/save/delete image functions
static Boolean OpenFormHandleEvent(EventPtr eP);
void OpenFormInit();
static Boolean SaveFormHandleEvent(EventPtr eP);
void SaveFormInit();
static Boolean DeleteFormHandleEvent(EventPtr eP);
void DeleteFormInit();

//windowing functions
int WIN_ShowDialogForm(int formID,FormEventHandlerType *handler);
void WIN_FillListWithImageNames(DmOpenRef db, int listID,int selectedID);

/***********************************************************************
 *   My Utility Functions
 *
 *	  Random functions that I needed at some point.
 ***********************************************************************/

//this returns the bit depth of the screen
int UTIL_GetScreenMode(){
	Err err;
	unsigned long newDepth;
	err = WinScreenMode(winScreenModeGet, NULL, NULL,&newDepth, NULL);
	return newDepth;
}

//this prints to the output field if DEBUG is true
void UTIL_Output(Char* str){
	if(DEBUG){
		FldSetTextPtr(outputFldP,str);
		FldDrawField(outputFldP);
	}
}

//I couldn't get Palm's StrItoS(int) to work, so I wrote my own
//	however, I'm too lazy to have it return the chars in order so
//	right now it is backwards :-)
Char *UTIL_itos(int n){
	if(n==0) return "0";
	int currDigit;
	Char *s = new Char[20];
	Char currChar;
	int c=0;
	while(n>0){
		currDigit = n % 10;
		n = n / 10;
		currChar = 48+currDigit;
		s[c] = currChar;
		c++;
	}
	if(c==0) {s[c]=0; c++;}
	s[c]=0;
	return s;
}

//Just called once, dumps the copy window and bitmap before exiting the app
void UTIL_CleanUpMemory(){
	Err error;
	//I already deleted the bufferWinH and bufferBmpP in DR_Copy()
	if (copyWinH) WinDeleteWindow(copyWinH,false);
	if (copyBmpP) error = BmpDelete(copyBmpP);
}

/***********************************************************************
 *	Metroworks Utility Functions
 *
 *	  I didn't write these.
 ***********************************************************************/

//this makes sure we have PalmOS 3.5 on the machine, very important
static Err RomVersionCompatible(UInt32 requiredVersion, UInt16 launchFlags)
{
	UInt32 romVersion;

	// See if we're on in minimum required version of the ROM or later.
	FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
	if (romVersion < requiredVersion)
		{
		if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
			(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
			{
			FrmAlert (RomIncompatibleAlert);
		
			// Palm OS 1.0 will continuously relaunch this app unless we switch to 
			// another safe one.
			if (romVersion < requiredVersion)
				{
				AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
				}
			}
		
		return sysErrRomIncompatible;
		}

	return errNone;
}

//this returns the pointer for an object on the current form
static void * GetObjectPtr(UInt16 objectID)
{
	FormPtr FrmG;

	FrmG = FrmGetActiveForm();
	return FrmGetObjectPtr(FrmG, FrmGetObjectIndex(FrmG, objectID));
}

/***********************************************************************
 *  My Drawing Stuff
 *
 *	  Note that I do all this by using the WinCopyRectangle(...) from
 *	  one window to another, which is why the copyBmp and bufferBmp
 *	  have windows associated with them
 **********************************************************************/

//Copy the drawing area to a temporary buffer, this is needed for
//	my undo functionality, among various other things
void DR_BufferScreen(){
	Err error;
	RectanglePtr tempRctP = new RectangleType;
	RctSetRectangle(tempRctP,0,0,160,160);					//copy the full window
	//delete the old buffer - this fixed an old problem of not buffering new pen-down events :-)
	if(bufferBmpP) error = BmpDelete(bufferBmpP);
	bufferBmpP = BmpCreate(160,160,8,NULL,&error);			//create the buffer bitmap
	if(bufferBmpP){
		bufferWinH = WinCreateBitmapWindow(bufferBmpP, &error);
		if(bufferWinH){
			WinCopyRectangle(WinGetDrawWindow(),bufferWinH,tempRctP,0,0,winPaint);	//copy the current drawing to the buffer
		}
	}
}

//Redraw whatever is in the drawing area buffer
void DR_DrawBuffer(){
	WinCopyRectangle(bufferWinH,WinGetDrawWindow(),drawAreaRctP,0,16,winPaint);	//copy the buffer back to the draw area
}

//Copy the selected area to the copyBmp, via a new copyWin
void DR_Copy(RectanglePtr rP){
	Err error;
	//initialize to the size we need
	copyBmpP = BmpCreate(rP->extent.x,rP->extent.y,8,NULL,&error);
	if(copyBmpP){			//make a window
		copyWinH = WinCreateBitmapWindow(copyBmpP, &error);
		if(copyWinH){		//copy it from the buffer
			WinCopyRectangle(bufferWinH,copyWinH,rP,0,0,winPaint);
		}
	}
}

//Paste the copied bmp at the point, make any white pixels transparent
//	if the pasteTransparent variable is true
void DR_Paste(PointType* tl){
	if(copyBmpP){
		if(pasteTransparent){
			copyBmpP->flags.hasTransparency = 1;
			copyBmpP->transparentIndex = 0;
		}
		WinDrawBitmap(copyBmpP,tl->x,tl->y);			//draw the copied bitmap
	}
}

/***********************************************************************
 *  My Animation Stuff
 **********************************************************************/

void AN_MoveToEdge(PointType *currPtP,int xStep,int yStep){
	DR_BufferScreen();
	int startTicks; int buff=2; int moveCount=0;
	RectanglePtr currLocRctP = new RectangleType;	//holds the loc to paste at
	RctSetRectangle(currLocRctP,currPtP->x,currPtP->y,copyBmpP->width,copyBmpP->height);
	UTIL_Output("moving....");
	while( RctPtInRectangle(currPtP->x+copyBmpP->width,currPtP->y+copyBmpP->height,animAreaRctP) &&
		   RctPtInRectangle(currPtP->x,currPtP->y,animAreaRctP) ){
		startTicks = TimGetTicks();
		while(TimGetTicks()<(startTicks+tpf)){}
		RctSetRectangle(currLocRctP,currPtP->x,currPtP->y,copyBmpP->width,copyBmpP->height);
		WinEraseRectangle(currLocRctP,0);
		DR_DrawBuffer();
		currPtP->x+=xStep; currPtP->y+=yStep;
		DR_Paste(currPtP);
		moveCount++;
		//UTIL_Output( UTIL_itos(moveCount) );
	}
	//bounce back
	RctSetRectangle(currLocRctP,currPtP->x,currPtP->y,copyBmpP->width,copyBmpP->height);
	WinEraseRectangle(currLocRctP,0);
	currPtP->x-=xStep; currPtP->y-=yStep;
	DR_Paste(currPtP);	
	UTIL_Output("done");
}

/***********************************************************************
 *  My OpenForm Functions
 *	  
 *	  This works by setting currImageID to the selected item from the list
 **********************************************************************/

//the event handler for my open form
static Boolean OpenFormHandleEvent(EventPtr eP){
	static firstTime = true;
	Boolean handled = false;
	WinDrawChar(55,10,155);
	
	if (firstTime) {
		//FrmDoDialog doesn't pop a FrmOpenEvent, so I have to do this myself
		OpenFormInit();
		firstTime=false;
	}
	
	switch (eP->eType) 
		{
		case ctlSelectEvent:
			firstTime = true;			//set up to call Init next time form is opened
			switch(eP->data.ctlSelect.controlID){
				case OpenOKButton:
					if(DmNumRecords(myBmpDB)!=0){	//List is not inited if no records, so check and skip
						ListType* myImageList = (ListType *) GetObjectPtr(OpenImageNameList);
						int selected = LstGetSelection(myImageList);
						if(selected==noListSelection) currImageID = -1;
						else currImageID = selected;
					}
					break;
			}
			break;

		default:
			break;
		}

	return handled;
}

//populate the list, no item preselected in the open dialog window
void OpenFormInit(){
	WIN_FillListWithImageNames(myBmpDB, OpenImageNameList,currImageID);
}

/***********************************************************************
 *  My SaveForm Functions
 *	  
 *	  This works replacing the bitmap selected in the database with the
 *	  current bufferBmp.  Also lets you make a new one (at the end of
 *	  the database).
 **********************************************************************/

//the event handler for my save form
static Boolean SaveFormHandleEvent(EventPtr eP){
	static firstTime = true;
	Boolean handled = false;
	WinDrawChar(55,10,155);
	
	if (firstTime) {
		//FrmDoDialog doesn't pop a FrmOpenEvent, so I have to do this myself
		SaveFormInit();
		firstTime=false;
	}
	
	switch (eP->eType) 
		{
		case ctlSelectEvent:
			firstTime = true;			//set up to call Init next time form is opened
			switch(eP->data.ctlSelect.controlID){
				case SaveOKButton:
					if(DmNumRecords(myBmpDB)!=0){	//if no records, don't show list
						ListType* myImageList = (ListType *) GetObjectPtr(SaveImageNameList);
						currImageID = LstGetSelection(myImageList);
						//this case shouldn't be possible
						if(currImageID == noListSelection) currImageID = -1;
					} else currImageID = -1;
					break;
				case SaveNewButton:
					currImageID = DmNumRecords(myBmpDB);	//add a new record to the end
					break;
			}
			break;

		default:
			break;
		}

	return handled;
}
 
//populate the list, preselect the last bitmap opened for convenience
void SaveFormInit(){
	WIN_FillListWithImageNames(myBmpDB, SaveImageNameList,currImageID);
}

/***********************************************************************
 *  My DelteForm Functions
 *
 *	  Similar to my other File forms, this lists the images and deletes
 *	  the one selected.  However, this uses deleteImageID because I
 *	  feared some kind of collision of "index out of range" error if the
 *	  user opens the SaveForm right after this.
 **********************************************************************/

//the event handler for my delete form
static Boolean DeleteFormHandleEvent(EventPtr eP){
	static firstTime = true;
	Boolean handled = false;
	WinDrawChar(55,10,155);
	
	if (firstTime) {
		//FrmDoDialog doesn't pop a FrmOpenEvent, so I have to do this myself
		DeleteFormInit();
		firstTime=false;
	}
	
	switch (eP->eType) 
		{
		case ctlSelectEvent:
			firstTime = true;			//set up to call Init next time form is opened
			switch(eP->data.ctlSelect.controlID){
				case DeleteOKButton:
					if(DmNumRecords(myBmpDB)!=0){	//List is not inited if no records, so check and skip
						ListType* myImageList = (ListType *) GetObjectPtr(DeleteImageNameList);
						int selected = LstGetSelection(myImageList);
						if(selected==noListSelection) deleteImageID = -1;
						else deleteImageID = selected;
					}
					break;
			}
			break;

		default:
			break;
		}

	return handled;
}

//populate the list, no item preselected in the open dialog window
void DeleteFormInit(){
	WIN_FillListWithImageNames(myBmpDB, DeleteImageNameList,currImageID);
}

/***********************************************************************
 *  My Windowing Functions
 *
 *	  These are random utility functions, just focused on handling UI
 *	  stuff centered around Windows.
 **********************************************************************/

//This fills the designated List with the names of the images in the
//	database.  Right now "name" means number in the DB.  Pass this a
//	selectedID of -1 if you don't want anything selected.  
void WIN_FillListWithImageNames(DmOpenRef db, int listID, int selectedID){
	ListType* myImageList = (ListType *) GetObjectPtr(listID);	//grab a pointer to the list
	int numImages = DmNumRecords(db);							//get the number of list items
	if(numImages==0) return;									//don't create if no items
	Char** imageNames = DM_ListSavedImages(db);					//make an array of the right size
	LstSetListChoices(myImageList,imageNames,numImages);		//fill it up with integers
	if(selectedID!=-1) LstSetSelection(myImageList,selectedID);	//preselect an item
	LstDrawList(myImageList);									//actually draw it
}

//Popup the designated form as a modal dialog box it would be nice if
//  I could do this in a way to trigger a FrmOpenEvent :-(
//	Doing it as a Dialog (FrmDoDialog) means that it is modal, and
//  returns the ID of the button pressed on the form popped up.
int WIN_ShowDialogForm(int formID,FormEventHandlerType *handler){
	int buttPressed;
	DR_BufferScreen();
	FormPtr frmP;
	MenuEraseStatus(0);					//clear the menu status from the display
	frmP = FrmInitForm (formID);
	FrmSetEventHandler(frmP, handler);	//set the event handler
	buttPressed = FrmDoDialog (frmP);	//display the form
	FrmDeleteForm (frmP);
	CLR_DrawSwatch();					//redraw the colors in case any were updated
	DR_DrawBuffer();					//is this needed?
	return buttPressed;					//return the ID of the button of the form pressed
}

/***********************************************************************
 *  My ColorPickerForm Functions
 *
 *	  These are functions that I use around my Color Picker form.  Yes,
 *	  Yes, I know Palm has their own built it, but I didn't know that
 *	  when I made this.  Anyway, it is clearly my goal to violate all the
 *	  design standards enforced by PalmOS, so I had to make my own :-)
 **********************************************************************/

//The event handler for my color picker form.  This is pretty closely
//	integrated with my color management system (the color swatch).
//  It works by copying the new color to the swatches id (0 if just
//  the foreground color).
static Boolean PickColorFormHandleEvent(EventPtr eP){
	static RectanglePtr clrSampleP = new RectangleType;
	static firstTime = true;
	Boolean handled = false;

	if (firstTime) {
		mode = MODE_PICK_COLOR;		//alter the mode
		//FrmDoDialog doesn't pop a FrmOpenEvent, so I have to do this myself
		PickColorFormInit();
		firstTime=false;
	}
	RctSetRectangle(clrSampleP,81,75,51,51);	//set the square that shows the current color
	
	switch (eP->eType) 
		{
		case frmOpenEvent:						//sadly, these two cases do nothing :-(
		case frmLoadEvent:						//  which is why I need to use the firstTime
			WinDrawChar(48,100,100);			//  variable... ugh
			break;
		case ctlSelectEvent:
			switch(eP->data.ctlSelect.controlID){
				case PickColorOKButton:
					firstTime = true;			//set up to call Init next time form is opened
					mode = MODE_DRAW;			//set the mode back
					break;
			}
			break;
		case ctlRepeatEvent:		//refresh the color box while draggin on a color bar
			switch (eP->data.ctlRepeat.controlID) {
				case PickColorPickRedFeedbackSliderControl:
					colors[currColorIndex].r = eP->data.ctlRepeat.value;
					break;
				case PickColorPickGreenFeedbackSliderControl:
					colors[currColorIndex].g = eP->data.ctlRepeat.value;
					break;
				case PickColorPickBlueFeedbackSliderControl:
					colors[currColorIndex].b = eP->data.ctlRepeat.value;
					break;
			}
			CLR_CopyToFore(currColorIndex);		//copy the new color to it's place
			WinSetForeColor( WinRGBToIndex( &colors[0] ) );	//set the new color
			WinDrawRectangle(clrSampleP,0);					//draw the color
			break;

		default:
			break;
		}

	return handled;
}

//This sets up my Color Picker to be used.
void PickColorFormInit(){
	//set the values of the sliders to the current foreground color
	//	I'm amazed this works...
	ControlType *slider;
	slider = (ControlType *) GetObjectPtr(PickColorPickRedFeedbackSliderControl);
	CtlSetValue(slider,colors[currColorIndex].r);
	slider = (ControlType *) GetObjectPtr(PickColorPickBlueFeedbackSliderControl);
	CtlSetValue(slider,colors[currColorIndex].b);
	slider = (ControlType *) GetObjectPtr(PickColorPickGreenFeedbackSliderControl);
	CtlSetValue(slider,colors[currColorIndex].g);
	//draw the current color on the left and right of the color selection form
	WinSetForeColor(WinRGBToIndex(&colors[currColorIndex]));
	RectanglePtr tempRctP = new RectangleType;
	RctSetRectangle(tempRctP,24,75,50,51);	//draw the old color
	WinDrawRectangle(tempRctP,0);	
	RctSetRectangle(tempRctP,81,75,51,51);	//draw the new color (currently same as old!)
	WinDrawRectangle(tempRctP,0);	
}

/***********************************************************************
 *  My Color Management Function
 *
 *	  These are utility functions specifically focused on my color
 *	  management system.
 **********************************************************************/

//Copy the color in colors[s] to the foreground (colors[0])
void CLR_CopyToFore(int s){
	colors[0].r = colors[s].r;
	colors[0].g = colors[s].g;
	colors[0].b = colors[s].b;
	currColorIndex = s;
	WinSetForeColor( WinRGBToIndex(&colors[0]) );
	if(mode!=MODE_PICK_COLOR) CLR_FillInEyeDropper(&colors[0]);
}

//Copy the color in colors[s] to the background (colors[1]).  I don't every use
//  this right now since the background color is always white.
void CLR_CopyToBack(int s){
	colors[1].r = colors[s].r;
	colors[1].g = colors[s].g;
	colors[1].b = colors[s].b;
	CLR_FillInEraser(&colors[1]);		//redraw the eraser with the right color
}

//Fill in the eraser with the current background color (stored in colors[1])
//  Again, this is obvisouly hugely tied to the interface.  But I don't ever
//  use this since the background is white.
void CLR_FillInEraser(RGBColorPtr clr){
	WinSetForeColor( WinRGBToIndex(&colors[1]) );	//set it to the background color
	WinDrawLine(81,10,86,10);
	WinDrawLine(81,9,86,9);
	WinDrawLine(82,7,87,7);
	WinDrawLine(83,6,88,6);
	WinDrawLine(84,5,89,5);
	WinDrawLine(85,4,90,4);
	WinDrawLine(88,8,91,5);
	WinDrawLine(88,9,91,6);
	WinSetForeColor( WinRGBToIndex(&colors[0]) );	//set it back to the foreground color	
}

//Draw the color select boxes on the screen.  This usually called after
//  one of them is changed.  Very tied to the interface.
void CLR_DrawSwatch(){
	RectanglePtr tempRct = new RectangleType;
	int i;
	for(i=0;i<5;i++){
		RctSetRectangle(tempRct, 96+i*13,3,9,9);	//where my boxes are
		WinSetForeColor( WinRGBToIndex(&colors[i+2]) );
		WinDrawRectangle(tempRct,0);				//draw the current color
	}
	CLR_FillInEyeDropper(&colors[0]);
	CLR_FillInEraser(&colors[10]);
}

//Fill in the eye dropper graphic with the color passed in.  This is,
//	obviously, very tied to the interface location of the eyeDropper
void CLR_FillInEyeDropper(RGBColorPtr clr){
	WinSetForeColor(WinRGBToIndex(clr));
	WinDrawLine(68,11,68,11);
	WinDrawLine(69,9,69,10);
	WinDrawLine(70,8,70,10);
	WinDrawLine(71,7,71,9);
	WinDrawLine(72,6,72,8);
	WinDrawLine(73,5,73,7);
	WinDrawLine(74,6,74,6);
}

/***********************************************************************
 * DATABASE STUFF
 *
 *	  This is the bitchy stuff that actually saves images to a database.
 *	  I pulled this method out of sample code from Palm's developer
 *	  site - however, I've forgotten the name of that app.  Not quite sure
 *	  how this works, but it does - and that's the important thing ;-)
 ***********************************************************************/

//This returns bitmap located at db(index).  Returns a NULL if none.
BitmapPtr DM_LoadBitmap(DmOpenRef db, int index){
	int size;
	MemHandle recMemH;
	MemPtr recMemP;
	BitmapPtr myBmpP;
	
	if(DmNumRecords(db)>0){							//if there are some bitmaps
		recMemH = DmGetRecord(db,index);
		if(recMemH){								//and this particular bitmap exists
			recMemP = MemHandleLock(recMemH);		//lock it
			size = MemPtrSize(recMemP);
			myBmpP = (BitmapPtr) MemPtrNew(size);	//make a new bitmap
			MemMove(myBmpP,recMemP,size);			//copy it over to the new bitmap
			MemHandleUnlock(recMemH);				//unlock it
			DmReleaseRecord(db,index,false);		//release the record - very important
			UTIL_Output("loadB: worked!");
			return myBmpP;							//return the new bitmap
		} else {
			UTIL_Output("loadB: getRecord failed");
		}
	} else {
		UTIL_Output("loadB: now records in db");
	}
	return NULL;
}

//This will save a bitmap to the specified index.  Note that this
//  overwrites any pre-existing bitmap at that location.
void DM_SaveBitmap(DmOpenRef db,unsigned short index, BitmapPtr myBmpP){
	int size;
	MemHandle recMemH;
	MemHandle oldMemH = NULL;
	MemHandle retMemH = NULL;
	MemPtr recMemP;
	Boolean exists = false;
	Err err;
	
	//if I don't have this if, it tossed an "index out of range error"...
	if(index!=DmNumRecords(db))
		oldMemH = DmGetRecord(db,index);
		if(oldMemH!=NULL){						//if a previous record existed
			exists = true;						//  mark a flag and 
			DmReleaseRecord(db,index,false);	//  release it, otherwise you're screwed!
		}

	if(myBmpP){									//if the bitmap to save exists
		size = MemPtrSize(myBmpP);				//size it up
		recMemH = DmNewHandle(db,size);			//make a new chunk for it
		if(!recMemH) {
			UTIL_Output("saveB: newHandle failed");
			return;
		}
		recMemP = MemHandleLock(recMemH);		//lock the chunk down
		DmWrite(recMemP,0,myBmpP,size);			//copy the bitmap over
		MemHandleUnlock(recMemH);				//free the memory, very important for overwritting
		if(exists) err = DmAttachRecord(db,&index,recMemH,&oldMemH);	//replace the old rec
		else  err = DmAttachRecord(db,&index,recMemH,NULL);				//or make a new one
		//BmpDelete(myBmpP);
		UTIL_Output("saveB: suceeded");
	} else {
		UTIL_Output("saveB: no bmp");
	}
}

//Delete the bitmap at index in db.  This assumes the record exists,
//  but it probably shouldn't - oh well!
void DM_DeleteBitmap(DmOpenRef db,int index){
	Err err;
	err = DmRemoveRecord(db,index);
}

//Return an array of strings that is a list of all the images.  This
//  is primarily used by WIN_FillListWithImageNames(...) to populate
//  a list on my file management dialogs.
Char** DM_ListSavedImages(DmOpenRef db){
	int numImages = DmNumRecords(db);
	Char** imageNames = new CharPtr[numImages];
	for(int i=0;i<numImages;i++){
		imageNames[i]=UTIL_itos(i);
	}
	return imageNames;
}

/***********************************************************************
 * MAIN FORM STUFF!
 *
 *	  These are the important functions that run most of the program!
 ***********************************************************************/

//Main form init stuff.  Mostly this sets up variables to be used later
static void MainFormInit(FormPtr /*FrmG*/){	
	DblClickDelay = SysTicksPerSecond()/2;
	clrCopyRectP->r = 220;   clrCopyRectP->g = 220;   clrCopyRectP->b = 220;
	//set up the selection rectangles to use later
	if(DEBUG){
		RctSetRectangle(drawAreaRctP,0,16,160,130);
		RctSetRectangle(animAreaRctP,2,18,158,128);
	} else {
		RctSetRectangle(drawAreaRctP,0,16,160,145);
		RctSetRectangle(animAreaRctP,2,18,158,143);	
	}
	//set up the animation speed
	int tps = SysTicksPerSecond();
	int fps = 5;
	tpf = (int) ((double)tps / (double)fps);
	bmpPtP = new PointType;						//initialize the loc register
	outputFldP = (FieldPtr) GetObjectPtr(MainOutputField);	//initialize the output field pointer
	DR_BufferScreen();	//need this to initialize bufferBmpP
	UTIL_Output(UTIL_itos(DmNumRecords(myBmpDB)));	//print the number of images currently stored
	CLR_DrawSwatch();
	CLR_FillInEyeDropper(&colors[0]);
}

//Main form menu handler.  This is where everything gets done.  I
//  should probably move the functionality out of some of these cases
//  and just call a nice little function, but that is too much effort.
static Boolean MainFormDoCommand(UInt16 command)
{
	Boolean handled = false;
	int buttPressed;

	switch (command)
		{
	  //PP menu  (About PalmPaint, Postcardware)
		case PPAboutPalmPaint:
			WIN_ShowDialogForm(AboutForm,NULL);
			handled = true;
			break;
		case PPPostcardware:
			WIN_ShowDialogForm(PostcardwareForm,NULL);
			handled = true;
			break;
	  //File menu (Open, Save, Delete, Pick Color)
		case FileOpen:
			buttPressed = WIN_ShowDialogForm(OpenForm,OpenFormHandleEvent);	//show the form
			if(buttPressed == OpenCancelButton) break;	//dismiss on a cancel
			if(currImageID!=-1){						//otherwire
				UTIL_Output(UTIL_itos(currImageID));
				BmpDelete(bufferBmpP);					//delete the buffer - very important
				bufferBmpP = DM_LoadBitmap(myBmpDB,currImageID);
				DR_DrawBuffer();						//load and draw the selected bitmap
			} else UTIL_Output("no selection");
			handled = true;
			break;
		case FileSave:
			DR_BufferScreen();							//grab any changes drawn
			buttPressed = WIN_ShowDialogForm(SaveForm,SaveFormHandleEvent);	//show the form
			if(buttPressed == SaveCancelButton) break;		//dismiss on a cancel
			if(currImageID == -1){		//no records in DB or no selection from list (which shouldn't be possible)
				UTIL_Output("save: no selection");
				currImageID = DmNumRecords(myBmpDB);	//add to the end
			} else {
				UTIL_Output(UTIL_itos(currImageID));	//could be a new one...
			}
			DM_SaveBitmap(myBmpDB,currImageID,bufferBmpP);	//save it
			handled = true;
			break;
		case FileDelete:
			buttPressed = WIN_ShowDialogForm(DeleteForm,DeleteFormHandleEvent);	//show the form
			if(buttPressed == DeleteCancelButton) break;	//dismiss on a cancel
			if(deleteImageID != -1){						//otherwise
				DM_DeleteBitmap(myBmpDB,deleteImageID);		//delete it!
			} else {
				UTIL_Output("delete: no selection");		//is this case possible?
			}
			handled = true;
			break;
	  //Edit menu (Undo, Copy, Paste, Paste transparent, Erase Screen)
		case EditUndo:
			DR_DrawBuffer();			//easy, just draw the last buffer since I buffer on penDowns
			handled = true;
			break;
		case EditCopy:
			mode = MODE_COPY;			//now it will do the selection rectangle stuff on Pen events
			DR_BufferScreen();			//copy to a buffer to not lose data, and to redraw inside the rect
			handled = true;
			break;
		case EditPaste:
			DR_BufferScreen();			//save this to be able to undo afterwards
			pasteTransparent=false;		//paste normally
			mode = MODE_PASTE;			//wait for a pen down and then paste
			handled = true;
			break;
		case EditPastetransparent:
			DR_BufferScreen();			//save this to be able to undo afterwards
			pasteTransparent=true;		//white will be see-through
			mode = MODE_PASTE;			//wait for a pen down and then paste
			handled = true;
			break;
	  //Draw Menu (Erase Screen, Empty Screen)
	  	case DrawEraseScreen:
	  		WinSetForeColor( WinRGBToIndex(&colors[1]) );	//set it to the background color
			WinDrawRectangle( drawAreaRctP,0 );				//fill the screen with the background color
	  		WinSetForeColor( WinRGBToIndex(&colors[0]) );	//set it to the foreground color
			handled = true;
	  		break;
		case DrawPickColor:
			currColorIndex=0;			//change the foreground color, not a swatch color
			WIN_ShowDialogForm(PickColorForm,PickColorFormHandleEvent);	//show the form
			//This is for using Palm's default color picker
				//Char *tP = "title"; Char *tipP = "fdsfa";
				//Boolean test = UIPickColor(NULL,&colors[0],NULL,NULL,NULL);
			handled = true;
			break;
	  //Debug Menu - this is taken out for release versions!
		case DebugBufferScreen:
			DR_BufferScreen();
			handled = true;
			break;
		case DebugDrawBuffer:
			DR_DrawBuffer();
			handled = true;
			break;
		case DebugUp:
			mode = MODE_ANIMATE;
			AN_MoveToEdge(bmpPtP,0,-1);
			mode = MODE_DRAW;
			handled = true;
			break;
		case DebugDown:
			mode = MODE_ANIMATE;
			AN_MoveToEdge(bmpPtP,0,1);
			mode = MODE_DRAW;
			handled = true;
			break;
		case DebugLeft:
			mode = MODE_ANIMATE;
			AN_MoveToEdge(bmpPtP,-1,0);
			mode = MODE_DRAW;
			handled = true;
			break;
		case DebugRight:
			mode = MODE_ANIMATE;
			AN_MoveToEdge(bmpPtP,1,0);
			mode = MODE_DRAW;
			handled = true;
			break;
		case DebugPrintDepth:
			UTIL_Output(UTIL_itos(UTIL_GetScreenMode()));
			break;
		}
			
	return handled;
}

//Main form event handler, pain in the ass
static Boolean MainFormHandleEvent(EventPtr eP)
{
	//need this now that Copy is a menu item to avoid triggering penup in MODE_COPY when menu item is selected
	static Boolean copying = false;
	static Boolean pasting = false;		//same for this and pasting
	//for drawing lines between pen-down events
	int startX,startY,endX,endY;
	static int oldX,oldY;
	double m;
	int width, i, yP;
	Boolean handled = false;
	FormPtr FrmG;
	//for tracking double-taps on erase or a color swatch
	static int currClickTicks=-1,lastClickTicks=-1,lastClickID=-1;
	Boolean dblClick = false;
	
	switch (eP->eType) 
		{
		case menuEvent:
			//return MainFormDoCommand(eP->data.menu.itemID);
			MainFormDoCommand(eP->data.menu.itemID);
			handled=true;
			break;

		case frmOpenEvent:
			FrmG = FrmGetActiveForm();
			MainFormInit( FrmG);
			FrmDrawForm ( FrmG);
			handled = true;
			break;

		case keyDownEvent:		//handle the graffitti shortcuts, I love how easy this was to add!
			unsigned char key = eP->data.keyDown.chr;
			//WinDrawChar(key,100,100);		//debugging
			switch(key){
			  //PalmPaint Menu
				case 'a': MainFormDoCommand(PPAboutPalmPaint); break;
				case 'w': MainFormDoCommand(PPPostcardware); break;
			  //File Menu
				case 'o': MainFormDoCommand(FileOpen); break;
				case 's': MainFormDoCommand(FileSave); break;
				case 'd': MainFormDoCommand(FileDelete); break;
			  //Edit Menu
				case 'u': MainFormDoCommand(EditUndo); break;
				case 'c': MainFormDoCommand(EditCopy); break;
				case 'v': MainFormDoCommand(EditPaste); break;
				case 't': MainFormDoCommand(EditPastetransparent); break;
			  //Draw Menu
				case 'p': MainFormDoCommand(DrawPickColor); break;
				case 'e': MainFormDoCommand(DrawEraseScreen); break;
			}
			handled = true;
			break;

		case ctlSelectEvent:
			handled = true;
			currClickTicks = TimGetTicks();
			switch(eP->data.ctlSelect.controlID){
				case MainArrowUpGraphicButton:				//increase the pen size
					if(PenSize < PEN_SIZE_MAX) PenSize++;
					break;
				case MainArrowDownGraphicButton:			//decrease the pen size
					if(PenSize > PEN_SIZE_MIN) PenSize--;
					break;
				case MainEyeDropperGraphicButton:			//set up to select the color of the pixel clicked on
					mode = MODE_GET_COLOR;
					CLR_FillInEyeDropper(&colors[0]);		//you have to redraw it after a click on it
					//right now this does nothing on a double-tap, should it?
					break;
				case MainEraseGraphicButton:
					CLR_CopyToFore(1);			//copy the background color to the foreground color
					UTIL_Output("erase mode");
					//erase on dblClick
					if((lastClickID==MainEraseGraphicButton) && (currClickTicks<(DblClickDelay+lastClickTicks))){
						DR_BufferScreen();
						WinSetForeColor( WinRGBToIndex(&colors[1]) );	//set it to the background color
						WinDrawRectangle(drawAreaRctP,0);
						UTIL_Output("erase screen");
						WinSetForeColor( WinRGBToIndex(&colors[0]) );	//set it back to the old foreground color
					}
					//CLR_FillInEraser(&colors[1]);		//you have to redraw it after a click on it
					break;

				//set the fore color to the one they picked, select a new color on a double-tap
				case MainColor1Button:
					UTIL_Output("color 1");
					//edit the swatch on a double-tap
					if((lastClickID==MainColor1Button) && (currClickTicks<(DblClickDelay+lastClickTicks))){
						WIN_ShowDialogForm(PickColorForm,PickColorFormHandleEvent);	//I like my own picker better!
						UTIL_Output("dblClick on color 1");
					}	
					currColorIndex = 2;
					CLR_CopyToFore(2);
					break;
				case MainColor2Button:
					UTIL_Output("color 2");
					//edit the swatch on a double-tap
					if((lastClickID==MainColor2Button) && (currClickTicks<(DblClickDelay+lastClickTicks))){
						WIN_ShowDialogForm(PickColorForm,PickColorFormHandleEvent);	//I like my own picker better!
						UTIL_Output("dblClick on color 2");
					}	
					currColorIndex = 3;
					CLR_CopyToFore(3);
					break;
				case MainColor3Button:
					UTIL_Output("color 3");
					//edit the swatch on a double-tap
					if((lastClickID==MainColor3Button) && (currClickTicks<(DblClickDelay+lastClickTicks))){
						WIN_ShowDialogForm(PickColorForm,PickColorFormHandleEvent);	//I like my own picker better!
						UTIL_Output("dblClick on color 3");
					}	
					currColorIndex = 4;
					CLR_CopyToFore(4);
					break;
				case MainColor4Button:
					UTIL_Output("color 4");
					//edit the swatch on a double-tap
					if((lastClickID==MainColor4Button) && (currClickTicks<(DblClickDelay+lastClickTicks))){
						WIN_ShowDialogForm(PickColorForm,PickColorFormHandleEvent);	//I like my own picker better!
						UTIL_Output("dblClick on color 4");
					}	
					currColorIndex = 5;
					CLR_CopyToFore(5);
					break;
				case MainColor5Button:
					UTIL_Output("color 5");
					//edit the swatch on a double-tap
					if((lastClickID==MainColor5Button) && (currClickTicks<(DblClickDelay+lastClickTicks))){
						WIN_ShowDialogForm(PickColorForm,PickColorFormHandleEvent);	//I like my own picker better!
						UTIL_Output("dblClick on color 5");
					}	
					currColorIndex = 6;
					CLR_CopyToFore(6);
					break;
				
			}
			if(!dblClick) lastClickTicks = currClickTicks;	//don't allow for triple-tap
			lastClickID = eP->data.ctlSelect.controlID;
			break;
			
		case penDownEvent:
			if(RctPtInRectangle(eP->screenX,eP->screenY,drawAreaRctP)){
				DR_BufferScreen();
				switch (mode){
					case MODE_DRAW:										//DRAWING
					  if(RctPtInRectangle(eP->screenX-PenSize/2,eP->screenY-PenSize/2,drawAreaRctP)){
						oldX=eP->screenX;
						oldY=eP->screenY;
						//reset the rectangle and draw the circle
						RectanglePtr rpP = new RectangleType;
						RctSetRectangle(rpP,eP->screenX-PenSize/2,eP->screenY-PenSize/2,PenSize,PenSize);
						WinDrawRectangle(rpP,PenSize/2);
					  }
						break;
					case MODE_COPY:										//SELECTING
						UTIL_Output("started copying...");
						copying=true;
						WinSetForeColor( WinRGBToIndex(clrCopyRectP) );
						oldX = eP->screenX;			//set up to draw the selection rectangle
						oldY = eP->screenY;
						break;
					case MODE_PASTE:									//PASTING
						pasting = true;
						break;
				}
			}
			break;
			
		case penMoveEvent:
			if(RctPtInRectangle(eP->screenX,eP->screenY,drawAreaRctP)){
				switch (mode){
					case MODE_DRAW:										//DRAWING
					  if(oldX==-1 && oldY==-1) break;
					  if(RctPtInRectangle(eP->screenX-PenSize/2,eP->screenY-PenSize/2,drawAreaRctP)){
						//draw the lines....
						WinSetForeColor( WinRGBToIndex(&colors[0]) );
						//RctSetRectangle(drawAreaRctP,0,16+PenSize/2,160,160-16-PenSize/2);
						RectanglePtr rP = new RectangleType;
						RctSetRectangle(rP,eP->screenX-PenSize/2,eP->screenY-PenSize/2,PenSize,PenSize);
						WinDrawRectangle(rP,PenSize/2);					//draw the circle
					  //Let the funky math begin!  There has to be a better way to do this...
						m =  (double) (eP->screenY-oldY) / (double) (eP->screenX-oldX);			//---------X
						//WinSetForeColor( WinRGBToIndex(clrGray) );
						startX = (eP->screenX > oldX) ? oldX : eP->screenX;
						startY = (eP->screenX > oldX) ? oldY : eP->screenY;
						width = (startX==oldX) ? eP->screenX-oldX : oldX-eP->screenX;
						for(i=0;i<width;i++){
							yP = (int) ( m * (double)i );
							RctSetRectangle(rP,(i+startX)-(PenSize/2),(yP+startY)-(PenSize/2),PenSize,PenSize);
							WinDrawRectangle(rP,PenSize/2);					//draw the circle
						}
						if(m>0.75 || m<-0.75){													//---------Y
							m =  (double) (eP->screenX-oldX) / (double) (eP->screenY-oldY);
							//WinSetForeColor( WinRGBToIndex(clrGray) );
							startY = (eP->screenY > oldY) ? oldY : eP->screenY;
							startX = (eP->screenY > oldY) ? oldX : eP->screenX;
							width = (startY==oldY) ? eP->screenY-oldY : oldY-eP->screenY;
							for(i=0;i<width;i++){
								yP = (int) ( m * (double)i );
								RctSetRectangle(rP,(yP+startX)-(PenSize/2),(i+startY)-(PenSize/2),PenSize,PenSize);
								WinDrawRectangle(rP,PenSize/2);					//draw the circle
							}
						}
						oldX=eP->screenX;			//store this loc as the old location
						oldY=eP->screenY;
					  }
						handled = false;
						break;
					case MODE_COPY:										//SELECTING
						//erase the old rectangle's frame
						WinEraseRectangleFrame(simpleFrame,selectionRctP);
						//increase the rectangle to cover the whole area under the rectangle and it's frame
						RctSetRectangle(selectionRctP,selectionRctP->topLeft.x-1,selectionRctP->topLeft.y-1,selectionRctP->extent.x+2,selectionRctP->extent.y+2);
						//copy back in what was there before to prevent any loss of image
						WinCopyRectangle(bufferWinH,WinGetDrawWindow(),selectionRctP,selectionRctP->topLeft.x,selectionRctP->topLeft.y,winPaint);
						//set the new selection rectangle
						startX = MIN(oldX,eP->screenX); endX = MAX(oldX,eP->screenX);
						startY = MIN(oldY,eP->screenY); endY = MAX(oldY,eP->screenY);
						RctSetRectangle(selectionRctP,startX,startY,endX-startX,endY-startY);
						//draw the new selection rectangle
						WinSetForeColor( WinRGBToIndex(clrCopyRectP) );
						WinDrawRectangleFrame(simpleFrame,selectionRctP);
						break;
				}
			}
			break;

		case penUpEvent:
			if(RctPtInRectangle(eP->screenX,eP->screenY,drawAreaRctP)){
				switch (mode){
					case MODE_DRAW:										//DRAWING
						oldX = -1;
						oldY = -1;
						break;
					case MODE_COPY:										//SELECTING
						if(copying){
							UTIL_Output("finishing copy...");
							//figure out the new selection rectangle
							startX = MIN(oldX,eP->screenX); endX = MAX(oldX,eP->screenX);
							startY = MIN(oldY,eP->screenY); endY = MAX(oldY,eP->screenY);
							RctSetRectangle(selectionRctP,startX,startY,endX-startX,endY-startY);
							DR_Copy(selectionRctP);					//copy it to the copyBmpP
							WinEraseRectangleFrame(simpleFrame,selectionRctP);	//erase the old selection rectangle
							mode = MODE_DRAW;						//go back to normal drawing mode
							UTIL_Output("finished copying...");
							DR_DrawBuffer();
							copying=false;
						}
						break;
					case MODE_PASTE:									//PASTING
						if(pasting){
							bmpPtP->x = eP->screenX; bmpPtP->y = eP->screenY;
							DR_Paste(bmpPtP);		//paste the image here!
							mode = MODE_DRAW;		//set it back to regular draw mode
							pasting=false;
						}
						break;
					case MODE_GET_COLOR:								//EYE DROPPER
						IndexedColorType pxClr = WinGetPixel(eP->screenX,eP->screenY);
						WinIndexToRGB(pxClr,&colors[0]);		//set the fore color to this pixel's color
						CLR_FillInEyeDropper(&colors[0]);		//redraw the erase to have the right color
						mode = MODE_DRAW;						//set it back to regular draw mode
						break;
				}
			}
			handled = true;
			break;
		
		default:
			break;
		
		}
	
	return handled;
}

/***********************************************************************
 * APPLICATION LEVEL Functions
 *
 *	I mostly just added stuff to ones that the Metroworks template created.
 *	I'm sure these can be collapsed into one function, but I'm afraid
 *	I would break something if I did that - so I'll leave them alone.
 ***********************************************************************/

//Basically this appears to just load up the main form when the
//	application starts up.  Seems to make sense to me!
static Boolean AppHandleEvent(EventPtr eP)
{
	UInt16 formId;
	FormPtr FrmG;

	if (eP->eType == frmLoadEvent)
		{
		// Load the form resource.
		formId = eP->data.frmLoad.formID;
		FrmG = FrmInitForm(formId);
		FrmSetActiveForm(FrmG);

		// Set the event handler for the form.  The handler of the currently
		// active form is called by FrmHandleEvent each time is receives an
		// event.
		switch (formId)
			{
			case MainForm:
				FrmSetEventHandler(FrmG, MainFormHandleEvent);
				break;

			default:
//				ErrFatalDisplay("Invalid Form Load Event");
				break;

			}
		return true;
		}
	
	return false;
}


//This sets up the hierarchy of handling events in the application.  Fairly clear.
static void AppEventLoop(void)
{
	UInt16 error;
	EventType event;

	do {
		EvtGetEvent(&event, evtWaitForever);

		if (! SysHandleEvent(&event))
			if (! MenuHandleEvent(0, &event, &error))
				if (! AppHandleEvent(&event))
					FrmDispatchEvent(&event);

	} while (event.eType != appStopEvent);
}

//Within this I load my preferences and my databases.  Of I end up creating
//	them if they don't exist (ie. when the user uses PalmPaint for the first
//	time.
static Err AppStart(void)
{
    UInt16 prefsSize;
    Err err = errNone;

	// Read the copyd preferences / copyd-state information.
	prefsSize = sizeof(PPPrefType);
	if (PrefGetAppPreferences(appFileCreator, appPrefID, &p, &prefsSize, true) != noPreferenceFound){
		//load the saved swatch
		colors[0].r = p.clr0.r;	colors[0].g = p.clr0.g;	colors[0].b = p.clr0.b;
		colors[1].r = p.clr1.r;	colors[1].g = p.clr1.g;	colors[1].b = p.clr1.b;
		colors[2].r = p.clr2.r;	colors[2].g = p.clr2.g;	colors[2].b = p.clr2.b;
		colors[3].r = p.clr3.r;	colors[3].g = p.clr3.g;	colors[3].b = p.clr3.b;
		colors[4].r = p.clr4.r;	colors[4].g = p.clr4.g;	colors[4].b = p.clr4.b;
		colors[5].r = p.clr5.r;	colors[5].g = p.clr5.g;	colors[5].b = p.clr5.b;
		colors[6].r = p.clr6.r;	colors[6].g = p.clr6.g;	colors[6].b = p.clr6.b;
	} else { 
		//no preferences, init to default colors - they'll get saved on exit
		colors[0].r = 0;   	colors[0].g = 0;	colors[0].b = 0;		//black - foreground
		colors[1].r = 255;  colors[1].g = 255;	colors[1].b = 255;		//white - background
		colors[2].r = 0;   	colors[2].g = 255;	colors[2].b = 0;		//green
		colors[3].r = 0;   	colors[3].g = 0;	colors[3].b = 0;		//black
		colors[4].r = 255;  colors[4].g = 255;	colors[4].b = 255;		//white
		colors[5].r = 255;  colors[5].g = 0;	colors[5].b = 0;		//red
		colors[6].r = 0;   	colors[6].g = 0;	colors[6].b = 255;		//blue
	}

	//open or create the database to hold bitmaps
	LocalID bmpDBid = DmFindDatabase(0,DB_NAME_BMP);
	if(bmpDBid==0){		//if it doesn't exist, create it and then open it
		err = DmCreateDatabase(0,DB_NAME_BMP,appFileCreator,DB_TYPE,false);
		if(err) return err;
		LocalID bmpDBid = DmFindDatabase(0,DB_NAME_BMP);
		myBmpDB = DmOpenDatabase(0,bmpDBid,dmModeReadWrite);
		if(!myBmpDB) return (1);
	} else {
		myBmpDB = DmOpenDatabase(0,bmpDBid,dmModeReadWrite);
	}

	//open or create the database to hold bitmap names
	if(stringDB){
		LocalID strDBid = DmFindDatabase(0,DB_NAME_STR);
		if(strDBid==0){		//if it doesn't exist, create it and then open it
			err = DmCreateDatabase(0,DB_NAME_STR,appFileCreator,DB_TYPE,false);
			if(err) return err;
			LocalID strDBid = DmFindDatabase(0,DB_NAME_STR);
			myStrDB = DmOpenDatabase(0,strDBid,dmModeReadWrite);
			if(!myStrDB) return (1);
		} else {
			myStrDB = DmOpenDatabase(0,strDBid,dmModeReadWrite);
		}
	}

	return err;
}

//This is where I save my stuff and close up shop.
static void AppStop(void)
{
	//set up the swatch to be saved
	p.clr0.r = colors[0].r;	p.clr0.g = colors[0].g;	p.clr0.b = colors[0].b;
	p.clr1.r = colors[1].r;	p.clr1.g = colors[1].g;	p.clr1.b = colors[1].b;
	p.clr2.r = colors[2].r;	p.clr2.g = colors[2].g;	p.clr2.b = colors[2].b;
	p.clr3.r = colors[3].r;	p.clr3.g = colors[3].g;	p.clr3.b = colors[3].b;
	p.clr4.r = colors[4].r;	p.clr4.g = colors[4].g;	p.clr4.b = colors[4].b;
	p.clr5.r = colors[5].r;	p.clr5.g = colors[5].g;	p.clr5.b = colors[5].b;
	p.clr6.r = colors[6].r;	p.clr6.g = colors[6].g;	p.clr6.b = colors[6].b;

	//clean up after myself
	UTIL_CleanUpMemory();
	
	// Write the copied preferences / copied-state information.  This data 
	// will be backed up during a HotSync.
	PrefSetAppPreferences (appFileCreator, appPrefID, appPrefVersionNum, 
		&p, sizeof (p), true);
	
	//Close the Databases
	if(stringDB) DmCloseDatabase(myStrDB);
	DmCloseDatabase(myBmpDB);
	
	// Close all the open forms.
	FrmCloseAllForms ();
}


/***********************************************************************
 * Application Entry Functions
 *
 *	This is all Metroworks stuff that I haven't touched.
 ***********************************************************************/

static UInt32 StarterPalmMain(UInt16 cmd, MemPtr /*cmdPBP*/, UInt16 launchFlags)
{
	Err error;

	error = RomVersionCompatible (ourMinVersion, launchFlags);
	if (error) return (error);

	switch (cmd)
		{
		case sysAppLaunchCmdNormalLaunch:
			error = AppStart();
			if (error) 
				return error;
				
			FrmGotoForm(MainForm);
			AppEventLoop();
			AppStop();
			break;

		default:
			break;

		}
	
	return errNone;
}

UInt32 PilotMain( UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    return StarterPalmMain(cmd, cmdPBP, launchFlags);
}
