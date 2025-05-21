/*
 * custom_cursors_no_frills.c
 *
 *  Created on: May 20, 2025
 *      Author: micahbly
 */

/* about
 *
 * INIT for Mac System 6.x through 9.x that adds alternate key layout when 
 *  specified modifer key is engaged.
 *
 * Works by patching the GetNextEvent trap (tail patch) and modifying the
 *  EventRecord if one of the desired keys has been typed with capslock on
 *
 * Primary reference: 
 *  http://preserve.mactech.com/articles/mactech/Vol.05/05.10/INITinC/index.html
 * 
 */

// Initially created in 2020
// Various variants since then, this one is based off "opt_cursors.c"
// Primary difference with this version is that the cursor keys and modifier 
//  are designed to be modified via ResEdit by the user, so variables are laid
//  out with that in mind.
// This specific variant is designed for Mac 128/512 with limmited memory
//  and running System 1.x - 3.x. It does not attempt to show an icon on boot


/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

// project includes


// C includes
#include <stdbool.h>
#include <stdint.h>

// Platform includes
#include <SetUpA4.h>
#include <Traps.h>


/*****************************************************************************/
/*                               Definitions                                 */
/*****************************************************************************/

#define GetNextEventTrap 			0xA970	// trap address in Mac 128/512/Plus

#define OPT_KEY_MASK				0x0800	// 0b01010000 00000000 = bits for both right option 0x4000 and general options 0x0800

#define MAP_IDX_UP					0	// pos within cursors_remap_key
#define MAP_IDX_LEFT				0	// pos within cursors_remap_key
#define MAP_IDX_DOWN				0	// pos within cursors_remap_key
#define MAP_IDX_RIGHT				0	// pos within cursors_remap_key

#define MODIFIER_OPT_KEY			0	// Option key
#define MODIFIER_CAPSLOCK_MODE_1	1	// CapsLock, keeping normal Caps behavior
#define MODIFIER_CAPSLOCK_MODE_2	2	// CapsLock, neutralizing normal Caps behavior


/*****************************************************************************/
/*                          File-scoped Variables                            */
/*****************************************************************************/

static int32_t		cursors_origGetNextEventAddr; // address of original GetNextEvent
static uint8_t		cursors_last_remapped_key = 0;
static bool			cursors_last_event_was_remap = false;

// ResEdit modification fun:
//  the four bytes after "KEYMAP>>" in ResEdit can be changed to whatever key you want
//  these are 1-byte codes, from page 251 of Inside Macintosh I.
//    for convenience, here are a few combos (in hex, for ResEdit):
//      =[]\: 18,21,1E,2A
//      IJKL: 22,26,28,25 (note: opt-i is a 2-part accent combo so you may experience weirdness with IJKL)
//      WASD: 0D,00,01,02
//      keypad 8456: 5B,56,57,58
//  The 5th byte can be 0 (option key), 1 (capslock), or 2 (capslock, with all 
//    normal capslock behavior removed: most useful if you pick cursor keys that
//    are not normally used in typing, such as numpad 8456 so you can just leave it on
//  The 8 bytes after the end padding are TWO-byte replacement codes
//    As set, they work for cursor keys, and most people will never modify them
//    but you *could* change them to other keys if that's helpful for some reason. 
//    e.g., map "`" key to ESC ($1B) or something. You cannot remap modifiers.
//    Note: I didn't have any luck so far getting "DEL" to work as forward delete
static uint8_t		cursors_start_pad[] = "KEYMAP>>";	// ResEdit marker
static uint8_t		cursors_key[4] = {0x18, 0x21, 0x1E, 0x2A};
static uint8_t		cursors_modifier_choice = MODIFIER_CAPSLOCK_MODE_2;
static uint8_t		cursors_end_pad[] = "<<KEYMAP";	// ResEdit marker
static uint16_t		cursors_remap[4] = {0x4D1E, 0x461C, 0x481F, 0x421D};
					// byte 0: // Mac 128K keyboard key from IM I-251
					// byte 1: // Mac charset value from IM I-247
					// 0x4D1E; // Up cursor + "RS"
					// 0x461C; // Left cursor + "FS"
					// 0x481F; // Down cursor + "US"
					// 0x421D; // Right cursor + "GS"

/*****************************************************************************/
/*                             Global Variables                              */
/*****************************************************************************/


/*****************************************************************************/
/*                       Private Function Prototypes                         */
/*****************************************************************************/

void main(void);

// Intercept key events for our specified key combinations and modify them to
// be cursor keys instead. For any other combo, pass thru keys without mod.
//   Calls ToolBox GetNextEvent, modifies EventRecord.message if appropriate
// @return	Returns true if toolbox GetNextEvent returned true (an event needs processing)
pascal Boolean NewGetNextEvent(short eventMask, EventRecord *theEvent);


/*****************************************************************************/
/*                       Private Function Definitions                        */
/*****************************************************************************/


// Intercept key events for our specified key combinations and modify them to
// be cursor keys instead. For any other combo, pass thru keys without mod.
//   Calls ToolBox GetNextEvent, modifies EventRecord.message if appropriate
// @return	Returns true if toolbox GetNextEvent returned true (an event needs processing)
pascal Boolean NewGetNextEvent(short eventMask, EventRecord *theEvent)
{
	uint8_t		the_key;
	uint8_t		the_char;	// needed for unshifting in capslock modes
	int16_t		i;
	uint32_t	modified_code_and_char = 0;
	bool		event_needs_action;
	bool		is_repeat_of_last;
	bool		do_remap;
	bool		modifier_down;

	// LOGIC:
	//   call original GetNextEvent()
	//   if the event is a keydown event, inspect modifier. if not Option key, return
	//   inspect the key. If [, ], \, or =, translate to a cursor key
	//   before returning, remove option key from the modifiers, but do not clear them. 
	//     this allows SHIFT-cursor-right etc.
	
	SetUpA4();

	// call original GetNextEvent
	event_needs_action = CallPascalB(eventMask, theEvent, cursors_origGetNextEventAddr);

	if (event_needs_action)
	{
		if (theEvent->what == keyDown || theEvent->what == autoKey)
		{
			// determine if selected modifier is down, then do universal check for the key
			// can't return even if modifier not down until we check for key repeat
			// key repeat events do not include the modifier key info!
			
			switch(cursors_modifier_choice)
			{
				case MODIFIER_CAPSLOCK_MODE_1:
				case MODIFIER_CAPSLOCK_MODE_2:
					modifier_down = ((theEvent->modifiers & alphaLock) > 0);
					break;
				
				case MODIFIER_OPT_KEY:
					modifier_down = ((theEvent->modifiers & optionKey) > 0);
					break;
				
				default:
					// modifier not down, but need to check if this is a repeat event before giving up
					break;
			}
			
			// LOGIC:
			//   do remapping if:
			//     (the modifier is down AND a specified key is down) OR
			//     (it is a key repeat event AND the repeat key matches one 
			//         of our keys AND we previously set flag that we are 
			//         remapping)
			
		    the_key = (theEvent->message & keyCodeMask) >> 8;
		    the_char = theEvent->message & charCodeMask;
			is_repeat_of_last = (the_key == cursors_last_remapped_key && cursors_last_event_was_remap);
			do_remap = ((modifier_down || is_repeat_of_last) > 0);
			
			if (do_remap == true)
		    {
				// LOGIC:
				//   we have array for key to map (1 byte)
				//   and array for key to map to (2 bytes)
				//   same offsets used for both, so no need to different 
				//   code per key (all are co-equal and get same simple swap)
				
				for (i = 0; i < 4; i++)
				{
					if (the_key == cursors_key[i])
					{
						modified_code_and_char = cursors_remap[i];
					}
				}
			
				if (modified_code_and_char)
				{
					// re-mask by blanking out lower 2 bytes, preserving 3rd/4th byte
					theEvent->message = (theEvent->message & 0xFFFF0000) | modified_code_and_char;

					// different behavior depending on modifier choice
					// note that MODIFIER_CAPSLOCK_MODE_2 is handled further down
					//  because it applies even if not working with a remap key
					
					if (cursors_modifier_choice == MODIFIER_OPT_KEY)
					{
						// clear the option modifier only, leaving any shift, control, etc.
						// this means that essentially, you can't do option [, ], = or \. boohoo.					
						theEvent->modifiers &= ~(OPT_KEY_MASK);
					}
					else if (cursors_modifier_choice == MODIFIER_CAPSLOCK_MODE_1)
					{
						// clear the capslock modifier only, leaving any shift, control, option, etc.
						theEvent->modifiers &= ~(alphaLock);
						
						// LOGIC:
						//   it is not necessary for us to remap upper to lower
						//   for capslock mode 1 because wee already remapped THIS key
						//   to a cursor key. Capsmode 2 below will remap chars to lower if necessar.
					}					
		
					cursors_last_remapped_key = the_key;
					cursors_last_event_was_remap = true;
				}
			}
			else
			{
				cursors_last_event_was_remap = false;
			}
			
			// for capslock mode 2 only: ALWAYS neutralize capslock on key down
			// even if for keys we aren't mapping. The goal is to let the user
			// just leave the capslock on permanently, and have cursors, but other-
			// wise totally normal key behavior. Obviously, not good for IJKL, but
			// good if you have a numpad and map to 8456 or 5123 etc.
			if (cursors_modifier_choice == MODIFIER_CAPSLOCK_MODE_2)
			{
				// clear the capslock modifier only, leaving any shift, control, option, etc.
				// this means there is no capslock-like behavior				
				theEvent->modifiers &= ~(alphaLock);

				// LOGIC: 
				//   The above will not actually accomplish much, other than
				//   letting any program testing for CapsLock know it isn't
				//   supposed to be on. The reason is that the keys have already been
				//   shifted by this point. Next thing we do is unshift alpha keys.
				//   note that we don't want to prevent caps if shift down
				
				if (the_char >= 'A' & the_char <= 'Z')
				{
					if ((theEvent->modifiers & shiftKey) < 1)
					{
						the_char += 32;	// diff between upper and lower in Mac ASCII
						theEvent->message = (theEvent->message & ~charCodeMask) | the_char;
					}
				}
			}
		}
	}
	
	RestoreA4();
	
	return event_needs_action;
}




/*****************************************************************************/
/*                        Public Function Definitions                        */
/*****************************************************************************/



// **** CONSTRUCTOR AND DESTRUCTOR *****



// **** SETTERS *****



// **** GETTERS *****



// **** OTHER FUNCTIONS *****

// Installs a patch to GetNextEvent so that all future calls route
//  through our private version first
void main(void)
{
	Handle		myHandle;
	Ptr			myPtr;
	SysEnvRec	world;
	Str255*		namePtr;

	// LOGIC:
	//  This block is called once. It saves the pointer
	//   to this code resource, and installs the patch.

	asm
	{
		move.l A0, myPtr
	}
	
 	RememberA0();
 	SetUpA4();
 	
 	if(!Button()) 
 	{
 		myHandle = RecoverHandle(myPtr); 		
		DetachResource(myHandle);

 		cursors_origGetNextEventAddr = NGetTrapAddress((int)GetNextEventTrap, ToolTrap);
		NSetTrapAddress((long)NewGetNextEvent, (int)GetNextEventTrap, ToolTrap);
	}
	
	RestoreA4();
}

