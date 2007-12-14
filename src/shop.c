/* 
 *
 *   Copyright (c) 2002, 2003 Johannes Prix
 *   Copyright (c) 2004-2007 Arthur Huillet 
 *
 *
 *  This file is part of Freedroid
 *
 *  Freedroid is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Freedroid is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Freedroid; see the file COPYING. If not, write to the 
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *  MA  02111-1307  USA
 *
 */

/* ----------------------------------------------------------------------
 * This file contains all menu functions and their subfunctions
 * ---------------------------------------------------------------------- */

#define _shop_c

#include "system.h"

#include "defs.h"
#include "struct.h"
#include "global.h"
#include "proto.h"
#include "SDL_rotozoom.h"

#define SHOP_ROW_LENGTH 8

typedef struct
{
  int shop_command;
  int item_selected;
  int number_selected;
}
shop_decision, *Shop_decision;

SDL_Rect ShopItemRowRect;
SDL_Rect TuxItemRowRect;

/* ----------------------------------------------------------------------
 * This function tries to buy the item given as parameter.  Currently
 * is just drops the item to the floor under the influencer and will
 * reduce influencers money.
 * ---------------------------------------------------------------------- */
int 
TryToPutItem( item* SellItem , int AmountToSellAtMost , moderately_finepoint pos )
{
  int i;
  int FreeIndex;

  while ( SpacePressed() || EnterPressed() );

  //--------------------
  // We catch the case, that not even one item was selected
  // for putting into a chest in the number selector...
  //
  if ( AmountToSellAtMost <= 0 ) 
    {
      DebugPrintf ( 0 , "\nTried to put 0 items of a kind into a chest or container... doing nothing... " );
      return ( FALSE );
    }

  if ( AmountToSellAtMost > SellItem -> multiplicity )
    AmountToSellAtMost = SellItem -> multiplicity ;

  //--------------------
  // At first we try to see if we can just add the multiplicity of the item in question
  // to the existing multiplicity of an item of the same type
  //
  if ( ItemMap [ SellItem->type ] . item_group_together_in_inventory )
    {
      for ( i = 0 ; i < MAX_ITEMS_PER_LEVEL ; i ++ )
	{
	  if ( curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . type == SellItem->type )
	    {
	      while ( 1 )
		{
		  while (EnterPressed() || SpacePressed() );

		  // PlayItemSound( ItemMap[ SellItem->type ].sound_number );
		  play_item_sound( SellItem -> type );

		  //--------------------
		  // We add the multiplicity to the one of the similar item found in the 
		  // box.
		  //
		  curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . multiplicity +=
		    AmountToSellAtMost;

		  //--------------------
		  // Depending on whether all has been put or just a small part of it,
		  // we either just reduce multiplicity or completely delete the item 
		  // on the part of the giving side.
		  //
		  if ( AmountToSellAtMost < SellItem->multiplicity )
		    SellItem->multiplicity -= AmountToSellAtMost;
		  else DeleteItem( SellItem );

		  return ( TRUE );
		}
	    }
	}
    }

  //--------------------
  // Not that we know that there is no item of the desired form present in
  // the container already, we will try to find a new item entry that we can
  // use for our purposes.
  //
  FreeIndex = (-1) ;
  for ( i = 0 ; i < MAX_ITEMS_PER_LEVEL ; i ++ )
    {
      if ( curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . type == (-1) )
	{
	  FreeIndex = i ;
	  break;
	}
    }

  //--------------------
  // If no free index was found, then of course we must cancel the whole 
  // operation.
  //
  if ( FreeIndex == (-1) )
    {
      ErrorMessage ( __FUNCTION__  , "\
The function used to put items into chests and containers encountered the\n\
case that there was no more room in the list of items in containers of this\n\
level.  We didn't think that this case would ever be reached, so we also didn't\n\
suitably handle it for now, i.e. we'll just quit, though it might be easy to\n\
write some suitable code here...",
				 PLEASE_INFORM, IS_FATAL );
      return ( FALSE );
    }

  //--------------------
  // Now we know that we have found a useable chest item index here
  // and we can take full advantage of it now.
  //
  CopyItem ( SellItem , & ( curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] ) , TRUE );
  curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . multiplicity = AmountToSellAtMost;
  curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . pos . x = pos . x ;
  curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . pos . y = pos . y ;
  if ( AmountToSellAtMost < SellItem->multiplicity )
    SellItem->multiplicity -= AmountToSellAtMost;
  else DeleteItem( SellItem );
  
  return ( TRUE );

}; // void TryToPutItem( ... )

/* ----------------------------------------------------------------------
 * This function prepares a new set of items, that will be displayed for
 * the Tux to buy.
 *
 * This function is different from the other 'AssemblePointerList..'
 * functions in the sense, that here we really must first CREATE the items.
 *
 * Since the shop will always COPY and DELETE items and not only point
 * some pointer to different directions, recurrent calls of this function
 * should not cause any damage, as long as there is only ONE player in 
 * the game.
 * ---------------------------------------------------------------------- */
void
AssembleItemListForTradeCharacter ( item* ListToBeFilled , int ShopCharacterCode )
{
    item* ListPointer = ListToBeFilled;
    int i;
    
    //--------------------
    // At first we clean out the given list.
    //
    ListPointer = ListToBeFilled;
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
	ListPointer->type = (-1) ;
	ListPointer->prefix_code = (-1) ;
	ListPointer->suffix_code = (-1) ;
	ListPointer++;
    }
    
    //--------------------
    // Depending on the character code given, we'll now refill the list
    // of items to be made available
    //
    ListPointer = ListToBeFilled;
    if ( ShopCharacterCode == PERSON_STONE )
	{
	ListPointer->type = GetItemIndexByName("Dagger"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Short sword"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Meat cleaver"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Crowbar"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Iron pipe"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Cap"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Buckler"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Simple Jacket"); ListPointer++;
	}
    else if ( ShopCharacterCode == PERSON_DOC_MOORE )
	{
	ListPointer->type = GetItemIndexByName("Diet supplement"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Antibiotic"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Doc-in-a-can"); ListPointer++;
	}
    else if ( ShopCharacterCode == PERSON_LUKAS )
	{
	ListPointer->type = GetItemIndexByName("Laser pistol"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Plasma pistol"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Laser Weapon Ammunition"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Plasma Weapon Ammunition"); ListPointer++;
	ListPointer->type = GetItemIndexByName(".22 LR Ammunition"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Shotgun shells"); ListPointer++;
	ListPointer->type = GetItemIndexByName("9x19mm Ammunition"); ListPointer++;
	ListPointer->type = GetItemIndexByName("7.62x39mm Ammunition"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Plasma Weapon Ammunition"); ListPointer++;
	ListPointer->type = GetItemIndexByName(".50 BMG (12.7x99mm) Ammunition"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Red Guard's Light Robe"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Red Guard's Heavy Robe"); ListPointer++;
	}
    else if ( ShopCharacterCode == PERSON_SKIPPY )
	{
	ListPointer->type = GetItemIndexByName("Map Maker"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Teleporter homing beacon"); ListPointer++;
	}
    else if ( ShopCharacterCode == PERSON_DUNCAN )
	{
	ListPointer->type = GetItemIndexByName("VMX Gas Grenade"); ListPointer++;
	ListPointer->type = GetItemIndexByName("EMP Shock Grenade"); ListPointer++;
//	ListPointer->type = GetItemIndexByName("EMP Shockwave Generator"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Plasma Grenade"); ListPointer++;
	}
    else if ( ShopCharacterCode == PERSON_EWALD )
	{
	ListPointer->type = GetItemIndexByName("Bottled ice"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Industrial coolant"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Liquid nitrogen"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Barf's Energy Drink"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Running Power Capsule"); ListPointer++;

	ListPointer->type = GetItemIndexByName("Fork"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Plate"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Mug"); ListPointer++;
	}
    else if ( ShopCharacterCode == PERSON_SORENSON )
	{
	ListPointer->type = GetItemIndexByName("Source Book of Emergency shutdown"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Check system integrity"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Sanctuary"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Analyze item"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Malformed packet"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Blue Screen"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Broadcast Blue Screen"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Calculate Pi"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Virus"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Broadcast virus"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Dispel smoke"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Killer poke"); ListPointer++;
//	ListPointer->type = GetItemIndexByName("Source Book of Reverse-engineer"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Plasma discharge"); ListPointer++;
	ListPointer->type = GetItemIndexByName("Source Book of Nethack"); ListPointer++;
//	ListPointer->type = GetItemIndexByName("Source Book of Invisibility"); ListPointer++;
//	ListPointer->type = GetItemIndexByName("Source Book of Ricer CFLAGS"); ListPointer++;
//	ListPointer->type = GetItemIndexByName("Source Book of Light"); ListPointer++;
//	ListPointer->type = GetItemIndexByName("Source Book of Satellite image"); ListPointer++;
	}
    else
	{
	ErrorMessage ( __FUNCTION__  , "\
		The function has received an unexpected character code.  This is not handled\n\
		currently and therefore initiates immediate termination now...",
		PLEASE_INFORM, IS_FATAL );
	}
    
    //--------------------
    // Now it's time to fill in the correct item properties and set
    // the right flags, so that we get a 'normal' item.
    //
    ListPointer = ListToBeFilled;
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
	if ( ListPointer->type == (-1) ) break;
	FillInItemProperties( ListPointer , TRUE ,  1);
	ListPointer -> is_identified = TRUE;
	ListPointer++;
    }
    
}; // void AssembleItemListForTradeCharacter ( .. )

/* ----------------------------------------------------------------------
 * At some points in the game, like when at the shop interface or at the
 * items browser at the console, we wish to show a list of the items 
 * currently in inventory.  This function assembles this list.  It lets
 * the caller decide on whether to include worn items in the list or not
 * and it will return the number of items finally filled into that list.
 * ---------------------------------------------------------------------- */
int
AssemblePointerListForItemShow ( item** ItemPointerListPointer , int IncludeWornItems )
{
  int i;
  item** CurrentItemPointer;
  int NumberOfItems = 0 ;

  //--------------------
  // First we clean out the new Show_Pointer_List
  //
  CurrentItemPointer = ItemPointerListPointer ;
  for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
      *CurrentItemPointer = NULL;
      CurrentItemPointer++;
    }

  //--------------------
  // Now we start to fill the Show_Pointer_List with the items
  // currently equipped, if that is what is desired by parameters...
  //
  CurrentItemPointer = ItemPointerListPointer;
  if ( IncludeWornItems )
    {
      if ( Me .weapon_item.type != ( -1 ) )
	{
	  *CurrentItemPointer = & ( Me .weapon_item );
	  CurrentItemPointer ++;
	  NumberOfItems ++;
	}
      if ( Me .drive_item.type != ( -1 ) )
	{
	  *CurrentItemPointer = & ( Me .drive_item );
	  CurrentItemPointer ++;
	  NumberOfItems ++;
	}
      if ( Me .armour_item.type != ( -1 ) )
	{
	  *CurrentItemPointer = & ( Me .armour_item );
	  CurrentItemPointer ++;
	  NumberOfItems ++;
	}
      if ( Me .shield_item.type != ( -1 ) )
	{
	  *CurrentItemPointer = & ( Me .shield_item );
	  CurrentItemPointer ++;
	  NumberOfItems ++;
	}
      if ( Me .special_item.type != ( -1 ) )
	{
	  *CurrentItemPointer = & ( Me .special_item );
	  CurrentItemPointer ++;
	  NumberOfItems ++;
	}
    }
  
  //--------------------
  // Now we start to fill the Show_Pointer_List with the items in the
  // pure unequipped inventory
  //
  for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
      if ( Me .Inventory [ i ].type == (-1) ) continue;
      else
	{
	  *CurrentItemPointer = & ( Me .Inventory[ i ] );
	  CurrentItemPointer ++;
	  NumberOfItems ++;
	}
    }
  
  return ( NumberOfItems );
  
}; // void AssemblePointerListForItemShow ( .. )
  
/* ----------------------------------------------------------------------
 * Before we enter the chest put/take interface menu, we must assemble
 * the list of pointers to the items currently in this chest.  An item
 * is in the chest at x/y if it's in the chest-item-list of the current
 * players level and if it's coordinates are those of the current players
 * chest.
 * ---------------------------------------------------------------------- */
int
AssemblePointerListForChestShow ( item** ItemPointerListPointer , moderately_finepoint chest_pos )
{
    int i;
    item** CurrentItemPointer;
    int NumberOfItems = 0 ;

    //--------------------
    // First we clean out the new Show_Pointer_List
    //
    CurrentItemPointer = ItemPointerListPointer ;
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
	*CurrentItemPointer = NULL;
	CurrentItemPointer++;
    }
    
    CurrentItemPointer = ItemPointerListPointer;
    
    for ( i = 0 ; i < MAX_ITEMS_PER_LEVEL ; i ++ )
    {
	if ( curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . type == (-1) ) continue;
	
	//--------------------
	// All the items in chests within a range of 1 square around the Tux 
	// will be collected together to be shown in the chest inventory.
	//
	if ( sqrt ( ( chest_pos . x - curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . pos . x ) *
		    ( chest_pos . x - curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . pos . x ) +
		    ( chest_pos . y - curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . pos . y )  *
		    ( chest_pos . y - curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] . pos . y )  ) < 1 )
	{
	    *CurrentItemPointer = & ( curShip . AllLevels [ Me . pos . z ] -> ChestItemList [ i ] );
	    CurrentItemPointer ++;
	    NumberOfItems ++;
	}
    }
    
    return ( NumberOfItems );
    
}; // void AssemblePointerListForChestShow ( .. )
  
/* ----------------------------------------------------------------------
 * Maybe the user has clicked right onto the item overview row.  Then of
 * course we must find out and return the index of the item clicked on.
 * If no item was clicked on, then a -1 will be returned as index.
 * ---------------------------------------------------------------------- */
int
ClickWasOntoItemRowPosition ( int x , int y , int TuxItemRow )
{
    if ( TuxItemRow )
    {
	if ( y < TuxItemRowRect . y ) return (-1) ;
	if ( y > TuxItemRowRect . y + TuxItemRowRect.h ) return (-1) ;
	if ( x < TuxItemRowRect . x ) return (-1) ;
	if ( x > TuxItemRowRect . x + TuxItemRowRect.w ) return (-1) ;
	
	//--------------------
	// Now at this point we know, that the click really was in the item
	// overview row.  Therefore we just need to find out the index and
	// can return;
	//
	return ( ( x - TuxItemRowRect . x ) / ( INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ) );
    }
    else
    {
	if ( y < ShopItemRowRect . y ) return (-1) ;
	if ( y > ShopItemRowRect . y + ShopItemRowRect.h ) return (-1) ;
	if ( x < ShopItemRowRect . x ) return (-1) ;
	if ( x > ShopItemRowRect . x + ShopItemRowRect.w ) return (-1) ;
	
	//--------------------
	// Now at this point we know, that the click really was in the item
	// overview row.  Therefore we just need to find out the index and
	// can return;
	//
	return ( ( x - ShopItemRowRect . x ) / ( INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ) );
    }
}; // int ClickWasOntoItemRowPosition ( int x , int y , int TuxItemRow )

/* ----------------------------------------------------------------------
 * The item row in the shop interface (or whereever we're going to use it)
 * should display not only the rotating item display but also a row or a
 * column of the current equipment, so that some better overview is given
 * as well and the item can be better associated with it's in-game inventory
 * representation.  This function displays one such representation with 
 * the correct size to fit perfectly into the overview item row.
 * ---------------------------------------------------------------------- */
void
ShowRescaledItem ( int position , int TuxItemRow , item* ShowItem )
{
    SDL_Rect TargetRectangle = { 0, 0, INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640,  INITIAL_BLOCK_HEIGHT * GameConfig . screen_height / 480};
    static iso_image equipped_icon ;
    static int first_call = TRUE ;

    if ( first_call )
    {
	    char fpath[2048];

            find_file ( "mouse_cursor_0003.png" , GRAPHICS_DIR, fpath, 0 );
            get_iso_image_from_file_and_path ( fpath , & ( equipped_icon ) , FALSE ) ;
            if ( equipped_icon . surface == NULL )
            {
                fprintf ( stderr , "\nFull path used: %s." , fpath );
                ErrorMessage ( __FUNCTION__ , "\
Error loading flag image.",   
                                           PLEASE_INFORM, IS_FATAL );
            }
            if ( use_open_gl )
            {
               make_texture_out_of_surface ( & ( equipped_icon ) ) ;
            }
    first_call = FALSE ;
    }
   

    TuxItemRowRect . x = 55 * GameConfig . screen_width / 640;
    TuxItemRowRect . y = 410 * GameConfig . screen_height / 480 ;
    TuxItemRowRect . h = INITIAL_BLOCK_HEIGHT * GameConfig . screen_height / 480 ;
//    TuxItemRowRect . h = 64 ;
    TuxItemRowRect . w = INITIAL_BLOCK_WIDTH * SHOP_ROW_LENGTH * GameConfig . screen_width / 640  ;
//    TuxItemRowRect . w = 64 ;
    
    ShopItemRowRect . x = 55 * GameConfig . screen_width / 640 ;
    ShopItemRowRect . y = 10 * GameConfig . screen_height / 480 ;
    ShopItemRowRect . h = INITIAL_BLOCK_HEIGHT * GameConfig . screen_height / 480 ;
    ShopItemRowRect . w = INITIAL_BLOCK_WIDTH * SHOP_ROW_LENGTH * GameConfig . screen_width / 640 ;
//    ShopItemRowRect . h = 64 ;
//    ShopItemRowRect . w = 64 ;
    
    if ( TuxItemRow == 1 )
    {
	TargetRectangle . x = TuxItemRowRect . x + position * INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ;
	TargetRectangle . y = TuxItemRowRect . y ;
    }
    else if ( TuxItemRow == 0 )
    {
	TargetRectangle . x = ShopItemRowRect . x + position * INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ;
	TargetRectangle . y = ShopItemRowRect . y ;
    }
    else
    {
	TargetRectangle . x = ShopItemRowRect . x + position * INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ;
	TargetRectangle . y = TuxItemRow ;
    }
    our_SDL_blit_surface_wrapper( ItemMap [ ShowItem -> type ] . inv_image . scaled_surface_for_shop , NULL , Screen , &TargetRectangle );
    if ( item_is_currently_equipped( ShowItem) )
	{
	if ( use_open_gl )
	    {
            draw_gl_textured_quad_at_screen_position ( &equipped_icon , TargetRectangle . x + TargetRectangle . w - 24, TargetRectangle . y );
    	    }
        else
            {
            blit_iso_image_to_screen_position ( &equipped_icon , TargetRectangle . x + TargetRectangle . w -  24, TargetRectangle . y );
            }
	}
}; // void ShowRescaledItem ( int position , item* ShowItem )

/* ----------------------------------------------------------------------
 * This function does the item show when the user has selected item
 * show from the console menu.
 * ---------------------------------------------------------------------- */
int
GreatShopInterface ( int NumberOfItems , item* ShowPointerList[ MAX_ITEMS_IN_INVENTORY ] , 
		     int NumberOfItemsInTuxRow , item* TuxItemsList[ MAX_ITEMS_IN_INVENTORY] , 
		     shop_decision* ShopOrder , int ShowChestButtons )
{
    int Displacement=0;
    bool finished = FALSE;
    static int WasPressed = FALSE ;
    int i;
    int ClickTarget;
    static int RowStart=0;
    static int TuxRowStart=0;
    static int ItemIndex=0;
    static int TuxItemIndex=-1;
    int RowLength=SHOP_ROW_LENGTH;
    int TuxRowLength=SHOP_ROW_LENGTH;
    char GoldString[1000];
    SDL_Rect HighlightRect;
    int BuyButtonActive = FALSE ;
    int SellButtonActive = FALSE ;

    //--------------------
    // For the shop, we'll also try to use our own mouse cursor
    //
    make_sure_system_mouse_cursor_is_turned_off();

    //--------------------
    // We add some secutiry against indexing beyond the
    // range of items given in the list.
    //
    if ( RowLength > NumberOfItems ) RowLength = NumberOfItems;
    while ( ItemIndex >= NumberOfItems ) ItemIndex -- ;
    while ( RowStart + RowLength > NumberOfItems ) RowStart -- ;
    if ( RowStart < 0 ) RowStart = 0 ;
    
    if ( TuxRowLength > NumberOfItemsInTuxRow ) TuxRowLength = NumberOfItemsInTuxRow;
    while ( TuxItemIndex >= NumberOfItemsInTuxRow ) TuxItemIndex -- ;
    while ( TuxRowStart + TuxRowLength > NumberOfItemsInTuxRow ) TuxRowStart -- ;
    if ( TuxRowStart < 0 ) TuxRowStart = 0 ;
    
    if ( NumberOfItemsInTuxRow <= 0 ) 
	TuxItemIndex = (-1) ;
    if ( NumberOfItems <= 0 ) 
	ItemIndex = (-1) ;
    

    //--------------------
    // We initialize the text rectangle
    //
    Cons_Text_Rect . x = 258 * GameConfig . screen_width / 640 ;
    Cons_Text_Rect . y = 108 * GameConfig . screen_height / 480 ;
    Cons_Text_Rect . w = 346 * GameConfig . screen_width / 640 ;
    Cons_Text_Rect . h = 255 * GameConfig . screen_height / 480 ;
    
    Displacement = 0;

    while (!finished)
    {
	
	//--------------------
	// We limit the 'displacement', i.e. how far up and down one can
	// scroll the text of the item description up and down a bit, so
	// it cannot be scrolled away ad infinitum...
	//
	if ( Displacement < -500 ) Displacement = -500 ;
	if ( Displacement > 50 ) Displacement = 50 ;
	
	SDL_Delay ( 1 );
	ShopOrder -> shop_command = DO_NOTHING ;
	
	//--------------------
	// We show all the info and the buttons that should be in this
	// interface...
	//
	AssembleCombatPicture( USE_OWN_MOUSE_CURSOR | ONLY_SHOW_MAP );
	if ( ItemIndex >= 0 )
	    ShowItemInfo ( ShowPointerList [ ItemIndex ] , Displacement , FALSE , ITEM_BROWSER_SHOP_BACKGROUND_CODE , FALSE );
	else if ( TuxItemIndex >= 0 )
	    ShowItemInfo ( TuxItemsList [ TuxItemIndex ] , Displacement , FALSE , ITEM_BROWSER_SHOP_BACKGROUND_CODE , FALSE );
	else blit_special_background ( ITEM_BROWSER_SHOP_BACKGROUND_CODE );
	
	
	for ( i = 0 ; i < RowLength ; i++ )
	{
	    ShowRescaledItem ( i , FALSE , ShowPointerList [ i + RowStart ] );
	}

	for ( i = 0 ; i < TuxRowLength ; i++ )
	{
	    ShowRescaledItem ( i , TRUE , TuxItemsList [ i + TuxRowStart ] );
	}
	
	if ( ItemIndex >= 0 ) 
	{
	    HighlightRect . x = ( ShopItemRowRect . x + ( ItemIndex - RowStart ) * INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ) ;
	    HighlightRect . y = ShopItemRowRect . y ;
	    HighlightRect . w = INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ;
	    HighlightRect . h = INITIAL_BLOCK_HEIGHT * GameConfig . screen_height / 480 ;
	    HighlightRectangle ( Screen , HighlightRect );
	}
	if ( TuxItemIndex >= 0 )
	{
	    HighlightRect . x = ( TuxItemRowRect . x + ( TuxItemIndex - TuxRowStart ) * INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ) ;
	    HighlightRect . y = TuxItemRowRect . y ;
	    HighlightRect . w = INITIAL_BLOCK_WIDTH * GameConfig . screen_width / 640 ;
	    HighlightRect . h = INITIAL_BLOCK_HEIGHT * GameConfig . screen_height / 480 ;
	    HighlightRectangle ( Screen , HighlightRect );
	}
	
	// ShowGenericButtonFromList ( LEFT_SHOP_BUTTON );
	// ShowGenericButtonFromList ( RIGHT_SHOP_BUTTON );
	
	// ShowGenericButtonFromList ( LEFT_TUX_SHOP_BUTTON );
	// ShowGenericButtonFromList ( RIGHT_TUX_SHOP_BUTTON );
	
	if ( ItemIndex >= 0 )
	{
	    if ( ShowChestButtons == 2 )
		ShowGenericButtonFromList ( REPAIR_BUTTON );
	    else if ( ShowChestButtons > 0 )
		ShowGenericButtonFromList ( TAKE_BUTTON );
	    else ShowGenericButtonFromList ( BUY_BUTTON );
	    BuyButtonActive = TRUE; 
	    SellButtonActive = FALSE ;
	}
	else if ( TuxItemIndex >= 0 )
	{
	    SellButtonActive = FALSE;
	    if ( ShowChestButtons ) ShowGenericButtonFromList ( PUT_BUTTON );
	    else if ( calculate_item_sell_price( TuxItemsList [ TuxItemIndex ] ))
			{
			ShowGenericButtonFromList ( SELL_BUTTON );
		        SellButtonActive = TRUE; 
			}
	    BuyButtonActive = FALSE ;
	    
	    //--------------------
	    // If some stuff in the Tux inventory is currently highlighted, we might
	    // eventually show repair and identify buttons, but only if appropriate, which
	    // means if reapair/identify is applicable AND also we're in a shop and NOT IN
	    // SOME CHEST!!!!
	    //
	    if ( ! ShowChestButtons )
	    {
		if ( ( ItemMap [ TuxItemsList [ TuxItemIndex ] -> type ] . base_item_duration >= 0 ) &&
		     ( TuxItemsList [ TuxItemIndex ] -> max_duration > TuxItemsList [ TuxItemIndex ] -> current_duration ) )
		    ShowGenericButtonFromList ( REPAIR_BUTTON );
		
		if ( ! TuxItemsList [ TuxItemIndex ] -> is_identified )
		    ShowGenericButtonFromList ( IDENTIFY_BUTTON );
	    }
	    
	}
	else
	{
	    BuyButtonActive = FALSE ;
	    SellButtonActive = FALSE ;
	}

	//--------------------
	// We show the current amount of 'gold' or 'cyberbucks' the tux
	// has on him.  However we need to take into account the scaling
	// of the whole screen again for this.
	//
	sprintf ( GoldString , "%6d" , (int) Me . Gold );
	PutStringFont ( Screen , FPS_Display_BFont, 40 * GameConfig . screen_width / 640 , 
			370 * GameConfig . screen_height / 480 , GoldString );
	
	blit_our_own_mouse_cursor ( );
	our_SDL_flip_wrapper( Screen );
	
	if ( SpacePressed() || EscapePressed() || MouseLeftPressed() )
	{
	    if ( MouseCursorIsOnButton( DESCRIPTION_WINDOW_UP_BUTTON , GetMousePos_x() , GetMousePos_y() ) && MouseLeftPressed() && !WasPressed )
	    {
		MoveMenuPositionSound();
		Displacement += FontHeight ( GetCurrentFont () );
	    }
	    else if ( MouseCursorIsOnButton( DESCRIPTION_WINDOW_DOWN_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed )
	    {
		MoveMenuPositionSound();
		Displacement -= FontHeight ( GetCurrentFont () );
	    }
	    else if ( MouseCursorIsOnButton( ITEM_BROWSER_EXIT_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed )
	    {
		finished = TRUE;
		while (SpacePressed() || EscapePressed() || MouseLeftPressed());
	    }
	    else if ( MouseCursorIsOnButton( LEFT_TUX_SHOP_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed )
	    {
		if ( 0 < RowStart ) 
		{
		    RowStart --;
		    if ( ( ItemIndex != (-1) ) && ( ItemIndex >= RowStart + RowLength ) ) 
		    {
			Displacement = 0 ;
			ItemIndex --;
		    }
		}
		MoveMenuPositionSound();
		while (SpacePressed() ||EscapePressed() || MouseLeftPressed());
	    }
	    else if ( MouseCursorIsOnButton( RIGHT_TUX_SHOP_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed )
	    {
		if ( RowStart + RowLength < NumberOfItems ) 
		{
		    RowStart ++;
		    if ( ( ItemIndex != (-1) ) && ( ItemIndex < RowStart ) ) 
		    {
			Displacement = 0 ;
			ItemIndex++;
		    }
		}
		MoveMenuPositionSound();
		while (SpacePressed() ||EscapePressed() || MouseLeftPressed());
	    }
	    else if ( MouseCursorIsOnButton( LEFT_SHOP_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed )
	    {
		if ( 0 < TuxRowStart ) 
		{
		    TuxRowStart --;
		    if ( ( TuxItemIndex != (-1) ) && ( TuxItemIndex >= TuxRowStart + TuxRowLength ) ) 
		    {
			Displacement = 0 ;
			TuxItemIndex --;
		    }
		}
		MoveMenuPositionSound();
		while (SpacePressed() ||EscapePressed() || MouseLeftPressed());
	    }
	    else if ( MouseCursorIsOnButton( RIGHT_SHOP_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed )
	    {
		if ( TuxRowStart + TuxRowLength < NumberOfItemsInTuxRow ) 
		{
		    TuxRowStart ++;
		    if ( ( TuxItemIndex != (-1) ) && ( TuxItemIndex < TuxRowStart ) ) 
		    {
			TuxItemIndex ++ ;
			Displacement = 0 ;
		    }
		}
		MoveMenuPositionSound();
		while (SpacePressed() ||EscapePressed() || MouseLeftPressed());
	    }
	    else if ( ( ( ClickTarget = ClickWasOntoItemRowPosition ( GetMousePos_x()  , GetMousePos_y()  , FALSE ) ) >= 0 ) && MouseLeftPressed() && !WasPressed )
	    {
		if ( ClickTarget < NumberOfItems )
		{
		    ItemIndex = RowStart + ClickTarget ;
		    TuxItemIndex = (-1) ;
		    Displacement = 0 ;
		}
	    }
	    else if ( ( ( ClickTarget = ClickWasOntoItemRowPosition ( GetMousePos_x()  , GetMousePos_y()  , TRUE ) ) >= 0 ) && MouseLeftPressed() && !WasPressed )
	    {
		if ( ClickTarget < NumberOfItemsInTuxRow )
		{
		    TuxItemIndex = TuxRowStart + ClickTarget ;
		    ItemIndex = (-1) ;
		    Displacement = 0 ;
		}
	    }
	    else if ( MouseCursorIsOnButton( BUY_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed )
	    {
		if ( BuyButtonActive )
		{
		    ShopOrder -> item_selected = ItemIndex ;
		    ShopOrder -> shop_command = BUY_1_ITEM ;
		    if ( ( ItemMap [ ShowPointerList [ ItemIndex ] -> type ] . item_group_together_in_inventory ) &&
			 ( Me . Gold / ItemMap [ ShowPointerList [ ItemIndex ] -> type ] . base_list_price >= 1 ) )
		    {
			//--------------------
			// If this is a shops buy menu, then we calculate what the Tux could afford here,
			// otherwise we give the range of selection according to amount in chest/player inventory.
			//
			if ( ShowChestButtons == 1 )
			    ShopOrder -> number_selected = do_graphical_number_selection_in_range ( 0 , ShowPointerList [ ItemIndex ] -> multiplicity , ShowPointerList [ ItemIndex ] -> multiplicity ) ;
			else
			    ShopOrder -> number_selected = do_graphical_number_selection_in_range ( 0 , Me . Gold / ItemMap [ ShowPointerList [ ItemIndex ] -> type ] . base_list_price , 1) ;
		    }
		    else
		    {

			if ( ( ShowChestButtons == 1 ) && ( ShowPointerList [ ItemIndex ] -> multiplicity > 1 ) )
			{
			    ShopOrder -> number_selected = do_graphical_number_selection_in_range ( 0 , ShowPointerList [ ItemIndex ] -> multiplicity, ShowPointerList [ ItemIndex ] -> multiplicity ) ;
			}
			else
			    ShopOrder -> number_selected = 1;
		    }
		    while ( MouseLeftPressed() ) SDL_Delay(1);
		    return ( 0 );
		}
	    }
	    else if ( MouseCursorIsOnButton( SELL_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed )
	    {
		if ( SellButtonActive )
		{
		    ShopOrder -> item_selected = TuxItemIndex ;
		    ShopOrder -> shop_command = SELL_1_ITEM ;
		    
		    if ( ( ItemMap [ TuxItemsList [ TuxItemIndex ] -> type ] . item_group_together_in_inventory ) &&
			 ( TuxItemsList [ TuxItemIndex ] -> multiplicity > 1 ) )
		    {
			ShopOrder -> number_selected = do_graphical_number_selection_in_range ( 0 , TuxItemsList [ TuxItemIndex ] -> multiplicity , TuxItemsList [ TuxItemIndex ] -> multiplicity );
		    }
		    else
			ShopOrder -> number_selected = 1;
		    while ( MouseLeftPressed() ) SDL_Delay(1);
		    return ( 0 );
		}
	    }
	    else if ( MouseCursorIsOnButton( REPAIR_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && 
		      MouseLeftPressed() && !WasPressed && ( !ShowChestButtons ) )
	    {
		//--------------------
		// Reference to the Tux item list must only be made, when the 'highlight'
		// is really in the tux item row.  Otherwise we just get a segfault...
		//
		if ( TuxItemIndex > (-1) )
		{
		    //--------------------
		    // Of course the repair button should only have effect, if there is
		    // really something to repair (and therefore the button is shown at
		    // all further above.
		    //
		    if ( ( ItemMap [ TuxItemsList [ TuxItemIndex ] -> type ] . base_item_duration >= 0 ) &&
			 ( TuxItemsList [ TuxItemIndex ] -> max_duration > TuxItemsList [ TuxItemIndex ] -> current_duration ) )
		    {
			ShopOrder -> item_selected = TuxItemIndex ;
			ShopOrder -> shop_command = REPAIR_ITEM ;
			ShopOrder -> number_selected = 1;
			
			return ( 0 );
		    }
		}
	    }
	    else if ( MouseCursorIsOnButton( IDENTIFY_BUTTON , GetMousePos_x()  , GetMousePos_y()  ) && MouseLeftPressed() && !WasPressed && ( !ShowChestButtons ) )
	    {
		//--------------------
		// Reference to the Tux item list must only be made, when the 'highlight'
		// is really in the tux item row.  Otherwise we just get a segfault...
		//
		if ( TuxItemIndex > (-1) )
		{
		    if ( ! TuxItemsList [ TuxItemIndex ] -> is_identified )
		    {
			ShopOrder -> item_selected = TuxItemIndex ;
			ShopOrder -> shop_command = IDENTIFY_ITEM ;
			ShopOrder -> number_selected = 1;
			
			return ( 0 );
		    }
		}
	    }
	}
	
	
	
	WasPressed = MouseLeftPressed();
	
	if (UpPressed() || MouseWheelUpPressed())
	{
	    MoveMenuPositionSound();
	    while (UpPressed());
	    Displacement += FontHeight ( GetCurrentFont () );
	}
	if (DownPressed() || MouseWheelDownPressed())
	{
	    MoveMenuPositionSound();
	    while (DownPressed());
	    Displacement -= FontHeight ( GetCurrentFont () );
	}
	if (RightPressed() )
	{
	    MoveMenuPositionSound();
	    while (RightPressed());
	    // if ( ItemType < Me.type) ItemType ++;
	}
	if (LeftPressed() )
	{
	    MoveMenuPositionSound();
	    while (LeftPressed());
	    // if (ItemType > 0) ItemType --;
	}
	
	if ( EscapePressed() )
	{
	    while ( EscapePressed() );
	    return (-1);
	}
	
    } // while !finished 
    
    return ( -1 ) ;  // Currently equippment selection is not yet possible...
    
}; // int GreatShopInterface ( int NumberOfItems , item* ShowPointerList[ MAX_ITEMS_IN_INVENTORY ] )


/* ----------------------------------------------------------------------
 * This function tells us which item in the menu has been clicked upon.
 * It does not check for lower than 4 items in the menu available.
 * ---------------------------------------------------------------------- */
int
ClickedMenuItemPosition( void )
{
  int CursorX, CursorY;
  int i;

  CursorX = GetMousePos_x()  ; // this is already the position corrected for 16 pixels!!
  CursorY = GetMousePos_y()  ; // this is already the position corrected for 16 pixels!!

#define ITEM_MENU_DISTANCE 80
#define ITEM_FIRST_POS_Y 130
#define NUMBER_OF_ITEMS_ON_ONE_SCREEN 4

  //--------------------
  // When a character is blitted to the screen at x y, then the x and y
  // refer to the top left corner of the coming blit.  Using this information
  // we will define the areas where a click 'on the blitted text' has occured
  // or not.
  //
  if ( CursorY < ITEM_FIRST_POS_Y )
    return (-1);
  if ( CursorY > ITEM_FIRST_POS_Y + NUMBER_OF_ITEMS_ON_ONE_SCREEN * ITEM_MENU_DISTANCE )
    return (-1);

  for ( i = 0 ; i < NUMBER_OF_ITEMS_ON_ONE_SCREEN ; i++ )
    {
      if ( CursorY < ITEM_FIRST_POS_Y + ( i+1 ) * ITEM_MENU_DISTANCE ) return i;
    }
  
  //--------------------
  // At this point we've already determined and returned to right click-area.
  // if this point is ever reached, a severe error has occured, and Freedroid
  // should therefore also say so.
  //
  ErrorMessage ( __FUNCTION__  , "\
The MENU CODE was unable to properly resolve a mouse button press.",
				 PLEASE_INFORM, IS_FATAL );

  return ( 3 ); // to make compilers happy :)

}; // int ClickedMenuItemPosition( void )

/* ----------------------------------------------------------------------
 * There are numerous functions in shops where you have to select one out
 * of many pieces of equipment.  Therefore a function is provided, that 
 * should be able to perform this selection process with the user and also
 * check for unwanted events, like non-present items selected and that.
 *
 * Several pricing methods exist and can be given as parameter, useful
 * for using this function either for buy, sell, repair or identification
 * purposes.
 *
 * ---------------------------------------------------------------------- */
#define PRICING_FOR_BUY 1
#define PRICING_FOR_SELL 2
#define PRICING_FOR_IDENTIFY 3
#define PRICING_FOR_REPAIR 4
int 
DoEquippmentListSelection( char* Startstring , item* Item_Pointer_List[ MAX_ITEMS_IN_INVENTORY ] , int PricingMethod )
{
    int Pointer_Index=0;
    int i;
    int InMenuPosition = 0;
    int MenuInListPosition = 0;
    char DescriptionText[5000];
    float PriceFound = 0;
    
    //--------------------
    // First we make sure, that neither space nor Escape are
    // pressed in the beginning, so that a real menu selection
    // will be done.
    //
    while ( SpacePressed() || EscapePressed() );
    
    //--------------------
    // At first we count how many items are really in this list.
    // The last one is indicated by a NULL pointer following directly
    // after it.
    //
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i++ )
    {
	if ( Item_Pointer_List[ i ] == NULL ) 
	{
	    Pointer_Index = i ;
	    break;
	}
    }
    
    //--------------------
    // Maybe there is NO ITEM IN THE LIST AT ALL!
    // Then of course we are done, hehe, and return the usual 'back'.
    // Thats perfectly normal and ok.
    //
    if ( Pointer_Index == 0 )
    {
	DebugPrintf ( 0 , "\nDoEquippmentListSelection(..):  No item in given list.  Returning... " );
	return ( -1 );
    }
    
    //--------------------
    // Now we clean the list from any (-1) items, that might come from
    // an items just having been sold and therefore deleted from the list.
    //
    for ( i = 0 ; i < Pointer_Index ; i++ )
    {
	if ( Item_Pointer_List[ i ]->type == (-1 ) )
	{
	    DebugPrintf ( 0 , "\nNOTE:  DoEquippmentListSelection(...): Cleaning a '-1' type item from the Item_Pointer_List... " );
	    
	    CopyItem ( Item_Pointer_List[ Pointer_Index - 1 ] , Item_Pointer_List[ i ] , FALSE );
	    DeleteItem ( Item_Pointer_List[ Pointer_Index - 1 ] ); // this is for -1 cause of SOLD items
	    Item_Pointer_List[ Pointer_Index -1 ] = NULL;
	    Pointer_Index --;
	    
	    return ( DoEquippmentListSelection ( Startstring , Item_Pointer_List , PricingMethod ) );
	}
    }
    
    //--------------------
    // Now IN CASE OF REPAIR_PRICING, we clean the list from any items, that 
    // don't need repair.
    //
    // TAKE CARE, THAT ITEMS MUST NOT BE OVERWRITTEN, ONLY THE POINTERS MAY BE MANIPULATED!!
    // Or we would destroy the Tux' inventory list!
    //
    if ( PricingMethod == PRICING_FOR_REPAIR )
    {
	for ( i = 0 ; i < Pointer_Index ; i++ )
	{
	    if ( Item_Pointer_List[ i ]->current_duration == Item_Pointer_List[ i ]->max_duration ) 
	    {
		DebugPrintf ( 0 , "\nNOTE:  DoEquippmentListSelection(...): Cleaning an item that does not need repair from the pointer list... " );
		
		// CopyItem ( Item_Pointer_List[ Pointer_Index - 1 ] , Item_Pointer_List[ i ] , FALSE );
		Item_Pointer_List[ i ] = Item_Pointer_List[ Pointer_Index -1 ] ;
		Item_Pointer_List[ Pointer_Index -1 ] = NULL;
		Pointer_Index --;
		
		return ( DoEquippmentListSelection ( Startstring , Item_Pointer_List , PricingMethod ) );
	    }
	}
    }
    
    //--------------------
    // Now IN CASE OF IDENTIFY_PRICING, we clean the list from any items, that 
    // don't need to be identified.
    //
    // TAKE CARE, THAT ITEMS MUST NOT BE OVERWRITTEN, ONLY THE POINTERS MAY BE MANIPULATED!!
    // Or we would destroy the Tux' inventory list!
    //
    if ( PricingMethod == PRICING_FOR_IDENTIFY )
    {
	for ( i = 0 ; i < Pointer_Index ; i++ )
	{
	    if ( Item_Pointer_List[ i ]->is_identified ) 
	    {
		DebugPrintf ( 0 , "\nNOTE:  DoEquippmentListSelection(...): Cleaning an item that does not need to be identified from the pointer list... " );
		
		Item_Pointer_List[ i ] = Item_Pointer_List[ Pointer_Index -1 ] ;
		Item_Pointer_List[ Pointer_Index -1 ] = NULL;
		Pointer_Index --;
		
		return ( DoEquippmentListSelection ( Startstring , Item_Pointer_List , PricingMethod ) );
	    }
	}
    }
    
    
    
    //--------------------
    // Now we can perform the actual menu selection.
    // We will loop until a decision of one kind or the other
    // has been made.
    //
    while ( !SpacePressed() && !EscapePressed() )
    {
	// InitiateMenu( NULL );
	InitiateMenu( SHOP_BACKGROUND_IMAGE_CODE );
	
	//--------------------
	// Now we draw our selection of items to the screen, at least the part
	// of it, that's currently visible
	//
	DisplayText( Startstring , 50 , 50 + (0) * ITEM_MENU_DISTANCE , NULL , TEXT_STRETCH );
	
	// DisplayText( DescriptionText , 580 , 50 + ( 0 ) * 80 , NULL );
	for ( i = 0 ; ( (i < NUMBER_OF_ITEMS_ON_ONE_SCREEN) && ( Item_Pointer_List[ i + MenuInListPosition ] != NULL ) ) ; i++ )
	{
	    // DisplayText( ItemMap [ Repair_Pointer_List[ i + ]->type ].item_name , 50 , 50 + i * 50 , NULL );
	    // DisplayText( "\n" , -1 , -1, NULL );
	    GiveItemDescription( DescriptionText , Item_Pointer_List [ i + MenuInListPosition ] , TRUE );
	    DisplayText( DescriptionText , 50 , 50 + (i+1) * ITEM_MENU_DISTANCE , NULL , TEXT_STRETCH );
	    
	    //--------------------
	    // Now we print out the price for this item, depending of course
	    // on the context in which we display this item.
	    //
	    switch ( PricingMethod )
	    {
		case PRICING_FOR_SELL:
		    PriceFound = calculate_item_sell_price ( Item_Pointer_List [ i + MenuInListPosition ] ) ;
		    break;
		case PRICING_FOR_BUY:
		    PriceFound = calculate_item_buy_price ( Item_Pointer_List [ i + MenuInListPosition ] ) ;
		    break;
		case PRICING_FOR_IDENTIFY:
		    PriceFound = 100.0 ;
		    break;
		case PRICING_FOR_REPAIR:
		    PriceFound = calculate_item_repair_price ( Item_Pointer_List [ i + MenuInListPosition] );
		    break;
		default:
		    DebugPrintf( 0 , "ERROR:  PRICING METHOD UNSPECIFIED IN SHOP.C!!!\n\nTerminating...\n\n" );
		    Terminate ( ERR );
		    break;
		    
	    }
	    sprintf( DescriptionText , "%6.0f" , PriceFound );
	    DisplayText( DescriptionText , 560 , 50 + (i+1) * ITEM_MENU_DISTANCE , NULL , TEXT_STRETCH );
	}
	
	//--------------------
	// Now we add a 'BACK' button outside the normal item display area.
	// Where exactly we put this button is pretty much unimportant, cause any
	// click outside the items displayed will return -1 to the calling function
	// and that's the same as clicking directly on the back button.
	//
	// DisplayText( "BACK" , 580 , 50 + (i+1) * ITEM_MENU_DISTANCE , NULL );
	// CenteredPutStringFont( Screen , Message_BFont , 50 + (i+1) * ITEM_MENU_DISTANCE , " BACK " );
	//
	CenteredPutString( Screen , 50 + (i+1) * ITEM_MENU_DISTANCE , _(" BACK "));
	
	//--------------------
	// Now we draw the influencer as a cursor
	//
	blit_tux ( 10 , 50 + ( InMenuPosition + 1 ) * ITEM_MENU_DISTANCE);
	
	//--------------------
	//
	//
	our_SDL_flip_wrapper ( Screen );
	
	//--------------------
	// Maybe the cursor key up or cursor key down was pressed.  Then of
	// course the cursor must either move down or the whole menu must
	// scroll one step down, if that is still possible.
	// 
	// Mouse wheel action will be checked for further down.
	//
	if ( UpPressed() || MouseWheelUpPressed() )
	{
	    if ( InMenuPosition > 0 ) InMenuPosition --;
	    else 
	    {
		if ( MenuInListPosition > 0 )
		    MenuInListPosition --;
	    }
	    while ( UpPressed() );
	}
	if ( DownPressed() || MouseWheelDownPressed() )
	{
	    if ( ( InMenuPosition < NUMBER_OF_ITEMS_ON_ONE_SCREEN - 1 ) &&
		 ( InMenuPosition < Pointer_Index -1 ) )
	    {
		InMenuPosition ++;
	    }
	    else 
	    {
		if ( MenuInListPosition < Pointer_Index - NUMBER_OF_ITEMS_ON_ONE_SCREEN )
		    MenuInListPosition ++;
	    }
	    while ( DownPressed() );
	}      
	
    } // while not space pressed...
    
    if ( SpacePressed() || MouseLeftPressed() ) 
    {
	return ( InMenuPosition + MenuInListPosition ) ;
    }
    
    while ( SpacePressed() || EscapePressed() );
    
    return (-1); // just to make compilers happy :)
    
}; // int DoEquippmentListSelection( char* Startstring , item* Item_Pointer_List[ MAX_ITEMS_IN_INVENTORY ] )

/* ----------------------------------------------------------------------
 * This function repairs the item given as parameter.
 * ---------------------------------------------------------------------- */
void 
TryToRepairItem( item* RepairItem )
{
    char* MenuTexts[ 10 ];
    MenuTexts[0]=_("Yes");
    MenuTexts[1]=_("No");
    MenuTexts[2]="";
    
    while ( SpacePressed() || EnterPressed() || MouseLeftPressed() ) SDL_Delay(1);
    
    if ( calculate_item_repair_price ( RepairItem ) > Me . Gold )
    {
	MenuTexts[0]=_(" BACK ");
	MenuTexts[1]="";
        SetCurrentFont ( Menu_BFont );
	DoMenuSelection ( _("\n\nYou can't afford to have this item repaired! ") , MenuTexts , 1 , -1 , NULL );
	return;
    }
    
    Me . Gold -= calculate_item_repair_price ( RepairItem ) ;
    RepairItem->current_duration = RepairItem->max_duration;
    PlayOnceNeededSoundSample ( "effects/Shop_ItemRepairedSound_0.ogg" , FALSE , FALSE );
}; // void TryToRepairItem( item* RepairItem )

/* ----------------------------------------------------------------------
 * This function tries to identify the item given as parameter.  
 * ---------------------------------------------------------------------- */
void 
TryToIdentifyItem( item* IdentifyItem )
{
    char* MenuTexts[ 10 ];
    MenuTexts[0]=_("Yes");
    MenuTexts[1]=_("No");
    MenuTexts[2]="";
    
    while ( SpacePressed() || EnterPressed() || MouseLeftPressed());
    
    if ( 100 > Me.Gold )
    {
	MenuTexts[0]=_(" BACK ");
	MenuTexts[1]="";
        SetCurrentFont ( Menu_BFont );
	DoMenuSelection ( _("You can't afford to have this item identified! ") , MenuTexts , 1 , -1 , NULL );
	return;
    }
    
/** As lynx pointed out on irc, it's useless to ask for confirmation, since the user actually requested identification. */
	while (EnterPressed() || SpacePressed() );
	Me.Gold -= 100 ;
	IdentifyItem -> is_identified = TRUE ;
	PlayOnceNeededSoundSample ( "effects/Shop_ItemIdentifiedSound_0.ogg" , FALSE , FALSE );

    return;
}; // void TryToIdentifyItem( item* IdentifyItem )

/* ----------------------------------------------------------------------
 * This function tries to sell the item given as parameter.
 * ---------------------------------------------------------------------- */
void 
TryToSellItem( item* SellItem , int WithBacktalk , int AmountToSellAtMost )
{
    int MenuPosition;
    char linebuf[1000];
    
#define ANSWER_YES 1
#define ANSWER_NO 2
    
    char* MenuTexts[ 10 ];
    MenuTexts[0]=_("Yes");
    MenuTexts[1]=_("No");
    MenuTexts[2]="";
    
    //--------------------
    // We catch the case, that not even one item was selected
    // for buying in the number selector...
    //
    if ( AmountToSellAtMost <= 0 ) 
    {
	DebugPrintf ( 0 , "\nTried to sell 0 items of a kind... doing nothing... " );
	return;
    }
    
    //--------------------
    // First some error-checking against illegal values.  This should not normally
    // occur, but some items on the map are from very old times and therefore the
    // engine might have made some mistakes back then or also changes that broke these
    // items, so some extra care will be taken here...
    //
    if ( SellItem -> multiplicity < 1 )
    {
	ErrorMessage ( __FUNCTION__  , "\
An item sold seemed to have multiplicity < 1.  This might be due to some\n\
fatal errors in the engine OR it might be due to some items droped on the\n\
maps somewhere long ago still had multiplicity=0 setting, which should not\n\
normally occur with 'freshly' generated items.  Well, that's some dust from\n\
the past, but now it should be fixed and not occur in future releases (0.9.10\n\
or later) of the game.  If you encounter this message after release 0.9.10,\n\
please inform the developers...",
				   PLEASE_INFORM, IS_WARNING_ONLY );
    }
    
    if ( AmountToSellAtMost > SellItem -> multiplicity )
	AmountToSellAtMost = SellItem -> multiplicity ;
    
    while ( SpacePressed() || EnterPressed() );
    
    if ( WithBacktalk )
    {
	while ( 1 )
	{
	    GiveItemDescription( linebuf , SellItem , TRUE );
	    strcat ( linebuf , _("\n\n    Are you sure you wish to sell this/(some of these) item(s)?") );
	    MenuPosition = DoMenuSelection( linebuf , MenuTexts , 1 , -1 , NULL );
	    switch (MenuPosition) 
	    {
		case (-1):
		    return;
		    break;
		case ANSWER_YES:
		    while (EnterPressed() || SpacePressed() );
		    
		    Me . Gold += calculate_item_sell_price ( SellItem );
		    DeleteItem( SellItem );
		    PlayOnceNeededSoundSample ( "effects/Shop_ItemSoldSound_0.ogg" , FALSE , TRUE );
		    
		    return;
		    break;
		case ANSWER_NO:
		    while (EnterPressed() || SpacePressed() );
		    return;
		    break;
	    }
	}
    }
    else
    {
	//--------------------
	// Ok.  Here we silently sell the item.
	//
	Me . Gold += calculate_item_sell_price ( SellItem ) * 
	    ( (float) AmountToSellAtMost ) / ( (float) SellItem -> multiplicity ) ;
	if ( AmountToSellAtMost < SellItem -> multiplicity )
	    SellItem -> multiplicity -= AmountToSellAtMost;
	else 
	    DeleteItem( SellItem );
	
	PlayOnceNeededSoundSample ( "effects/Shop_ItemSoldSound_0.ogg" , FALSE , TRUE );
    }
}; // void TryToSellItem( item* SellItem )

/* ----------------------------------------------------------------------
 * This function tries to put an item into the inventory, either by adding
 * this items multiplicity to the multiplicity of an already present item
 * of the very same type or by allocating a new inventory item for this
 * new item and putting it there.
 *
 * In the case that both methods couldn't succeed, a FALSE value is 
 * returned to let the caller know, that this procedure has failed.
 * Otherwise TRUE will indicate that everything is ok and went well.
 * ---------------------------------------------------------------------- */
int
TryToIntegrateItemIntoInventory ( item* BuyItem , int AmountToBuyAtMost )
{
    int x, y;
    int FreeIndex;
    char linebuf[1000];
    int i;
    char* MenuTexts[ 10 ];
    
    //--------------------
    // At first we try to see if we can just add the multiplicity of the item in question
    // to the existing multiplicity of an item of the same type
    //
    if ( ItemMap [ BuyItem->type ] . item_group_together_in_inventory )
    {
	for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
	{
	    if ( Me . Inventory [ i ] . type == BuyItem->type )
	    {
		while ( 1 )
		{
		    while (EnterPressed() || SpacePressed() );
		    Me . Inventory [ i ] . multiplicity += AmountToBuyAtMost ;
		    
		    //--------------------
		    // This is new.  I hope it's not dangerous.
		    //
		    if ( AmountToBuyAtMost >= BuyItem->multiplicity )
			DeleteItem ( BuyItem );
		    else
			BuyItem->multiplicity -= AmountToBuyAtMost ;
		    return ( TRUE );
		}
	    }
	}
    }
    
    //--------------------
    // Now we must find out if there is an inventory position where we can put the
    // item in question.
    //
    FreeIndex = GetFreeInventoryIndex(  );
    
    for ( x = 0 ; x < INVENTORY_GRID_WIDTH ; x ++ )
    {
	for ( y = 0 ; y < INVENTORY_GRID_HEIGHT ; y ++ )
	{
	    if ( ItemCanBeDroppedInInv ( BuyItem->type , x , y ) )
	    {
		while ( 1 )
		{
		    while (EnterPressed() || SpacePressed() );
		    
		    CopyItem( BuyItem , & ( Me.Inventory[ FreeIndex ] ) , FALSE );
		    Me.Inventory[ FreeIndex ] . multiplicity = AmountToBuyAtMost ;
		    
		    Me.Inventory[ FreeIndex ].currently_held_in_hand = FALSE;
		    Me.Inventory[ FreeIndex ].inventory_position.x = x;
		    Me.Inventory[ FreeIndex ].inventory_position.y = y;
		    
		    //--------------------
		    // This is new.  I hope it's not dangerous.
		    //
		    if ( BuyItem -> multiplicity <= AmountToBuyAtMost ) DeleteItem ( BuyItem );
		    else BuyItem -> multiplicity -= AmountToBuyAtMost ;
		    
		    return ( TRUE );
		}
	    }
	}
    }
    
    //--------------------
    // If this point is ever reached, we know that an item has been selected 
    // for buying and could be bought, if only ONE HAD ENOUGH ROOM IN INVENTORY!!
    // Therefore a message must be displayed, saying what the problem is.
    //
    PlayOnceNeededSoundSample ( "Tux_Hold_On_I_0.ogg" , FALSE , FALSE );
    MenuTexts[0]=_(" BACK ");
    MenuTexts[1]="";
    GiveItemDescription( linebuf , BuyItem , TRUE );
    strcat ( linebuf , _("\n\n   No room for this item in inventory!") );
    DoMenuSelection( linebuf , MenuTexts , 1 , -1 , NULL );
    return ( FALSE );
    
}; // void TryToIntegrateItemIntoInventory ( item* BuyItem , int AmountToBuyAtMost )

/* ----------------------------------------------------------------------
 * This function tries to buy the item given as parameter.  Currently
 * is just drops the item to the floor under the influencer and will
 * reduce influencers money.
 * ---------------------------------------------------------------------- */
void 
TryToTakeItem( item* BuyItem , int AmountToBuyAtMost )
{
    int StoredItemType;
    
    StoredItemType = BuyItem -> type ;
    
    //--------------------
    // We catch the case, that not even one item was selected
    // for taking out from the chest in the number selector...
    //
    if ( AmountToBuyAtMost <= 0 ) 
    {
	DebugPrintf ( 0 , _("\nTried to take 0 items of a kind from chest or cointainer... doing nothing... ") );
	return;
    }
    
    //--------------------
    // We prevent some take-put-cheating here.  For buying items this must
    // NOT be done.
    //
    if ( AmountToBuyAtMost >= BuyItem -> multiplicity ) AmountToBuyAtMost = BuyItem -> multiplicity ;
    
    if ( TryToIntegrateItemIntoInventory ( BuyItem , AmountToBuyAtMost ) )
    {
	// PlayItemSound( ItemMap[ StoredItemType ].sound_number );
	play_item_sound( StoredItemType );
    }
}; // void TryToTakeItem( item* BuyItem , int AmountToBuyAtMost )

/* ----------------------------------------------------------------------
 * This function tries to buy the item given as parameter.  Currently
 * is just drops the item to the floor under the influencer and will
 * reduce influencers money.
 * ---------------------------------------------------------------------- */
void 
TryToBuyItem( item* BuyItem , int WithBacktalk , int AmountToBuyAtMost )
{
    int FreeIndex;
    char linebuf[1000];
    float PotentialPrice;
    
#define ANSWER_YES 1
#define ANSWER_NO 2
    
    char* MenuTexts[ 10 ];
    MenuTexts[0]=_("Yes");
    MenuTexts[1]=_("No");
    MenuTexts[2]="";
    
    DebugPrintf ( 0 , "\nTryToBuyItem (...):  function called." );
    
    //--------------------
    // We catch the case, that not even one item was selected
    // for buying in the number selector...
    //
    if ( AmountToBuyAtMost <= 0 ) 
    {
	DebugPrintf ( 0 , "\nTried to buy 0 items of a kind... doing nothing... " );
	return;
    }
    
    BuyItem -> multiplicity = AmountToBuyAtMost ;
    
    FreeIndex = GetFreeInventoryIndex(  );
    
    while ( SpacePressed() || EnterPressed() || MouseLeftPressed() );
    
    if ( calculate_item_buy_price ( BuyItem ) > Me . Gold )
    {
	if ( WithBacktalk )
	{
	    MenuTexts[0]=_(" BACK ");
	    MenuTexts[1]="";
	    GiveItemDescription( linebuf , BuyItem , TRUE );
	    strcat ( linebuf , _("\n\n    You can't afford to purchase this item!") );
            SetCurrentFont ( Menu_BFont );
	    DoMenuSelection( linebuf , MenuTexts , 1 , -1 , NULL );
	}
	return;
    }
    
    //--------------------
    // In the case that the item could be afforded in theory, we need to
    // calculate the price, then have the item integrated into the inventory
    // if that's possible, and if so, subtract the items price from the
    // current gold.
    //
    PotentialPrice = calculate_item_buy_price ( BuyItem ) ;
    
    if ( TryToIntegrateItemIntoInventory ( BuyItem , AmountToBuyAtMost ) )
    {
	Me.Gold -= PotentialPrice ;
	PlayOnceNeededSoundSample ( "effects/Shop_ItemBoughtSound_0.ogg" , FALSE , FALSE );
    }
    else
    {
	// bad luck.  couldn't store item in inventory, so no price paid...
    }
    
}; // void TryToBuyItem( item* BuyItem )

/* ----------------------------------------------------------------------
 * This is some preparation for the shop interface.  We assemble some
 * pointer list with the stuff Tux has to sell and the stuff the shop
 * has to offer.
 *
 *
 * ---------------------------------------------------------------------- */
void
InitTradeWithCharacter( int CharacterCode )
{
#define FIXED_SHOP_INVENTORY TRUE
#define NUMBER_OF_ITEMS_IN_SHOP 17
    // #define NUMBER_OF_ITEMS_IN_SHOP 4
    
    item SalesList[ MAX_ITEMS_IN_INVENTORY ];
    item* BuyPointerList[ MAX_ITEMS_IN_INVENTORY ];
    item* TuxItemsList[ MAX_ITEMS_IN_INVENTORY ];
    int i;
    int ItemSelected=0;
    shop_decision ShopOrder;
    int NumberOfItemsInTuxRow=0;
    int NumberOfItemsInShop=0;
    
    AssembleItemListForTradeCharacter ( & (SalesList[0]) , CharacterCode );
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
	if ( SalesList [ i ] . type == ( - 1 ) ) BuyPointerList [ i ] = NULL ;
	else BuyPointerList [ i ] = & ( SalesList[ i ] ) ;
    }
    
    //--------------------
    // Now here comes the new thing:  This will be a loop from now
    // on.  The buy and buy and buy until at one point we say 'BACK'
    //
    while ( ItemSelected != (-1) )
    {
	
	NumberOfItemsInTuxRow = AssemblePointerListForItemShow ( & ( TuxItemsList [ 0 ] ) , TRUE );
	
	for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
	{
	    if ( BuyPointerList [ i ] == NULL )
	    {
		NumberOfItemsInShop = i ;
		break;
	    }
	}
	
	ItemSelected = GreatShopInterface ( NumberOfItemsInShop , BuyPointerList , 
					    NumberOfItemsInTuxRow , TuxItemsList , & ( ShopOrder ) , FALSE );
	switch ( ShopOrder . shop_command )
	{
	    case BUY_1_ITEM:
		TryToBuyItem( BuyPointerList[ ShopOrder . item_selected ] , TRUE , ShopOrder . number_selected ) ;
		break;
	    case BUY_10_ITEMS:
		TryToBuyItem( BuyPointerList[ ShopOrder . item_selected ] , FALSE , 10 ) ;
		break;
	    case BUY_100_ITEMS:
		TryToBuyItem( BuyPointerList[ ShopOrder . item_selected ] , FALSE , 100 ) ;
		break;
	    case SELL_1_ITEM:
		TryToSellItem( TuxItemsList[ ShopOrder . item_selected ] , FALSE , ShopOrder . number_selected ) ;
		break;
	    case SELL_10_ITEMS:
		TryToSellItem( TuxItemsList[ ShopOrder . item_selected ] , FALSE , 10 ) ;
		break;
	    case SELL_100_ITEMS:
		TryToSellItem( TuxItemsList[ ShopOrder . item_selected ] , FALSE , 100 ) ;
		break;
	    case REPAIR_ITEM:
		TryToRepairItem( TuxItemsList[ ShopOrder . item_selected ] );
		break;
	    case IDENTIFY_ITEM:
		TryToIdentifyItem( TuxItemsList[ ShopOrder . item_selected ] );
		break;
	    default:
		
		break;
	};
	
	//--------------------
	// And since it can be assumed that the shop never runs
	// out of supply for a certain good, we can as well restore
	// the shop inventory list at this position.
	//
	/*
	  if ( FIXED_SHOP_INVENTORY )
	  {
	  for ( i = 0 ; i < NUMBER_OF_ITEMS_IN_SHOP ; i++ )
	  {
	  SalesList[ i ].type = StandardShopInventory [ i ];
	  SalesList[ i ].prefix_code = ( -1 );
	  SalesList[ i ].suffix_code = ( -1 );
	  FillInItemProperties( & ( SalesList[ i ] ) , TRUE , 0 );
	  Buy_Pointer_List [ i ] = & ( SalesList[ i ] ) ;
	  }
	  Buy_Pointer_List [ i ] = NULL ; 
	  }
	*/
	AssembleItemListForTradeCharacter ( & ( SalesList [ 0 ] ) , CharacterCode );
	for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
	{
	    if ( SalesList [ i ] . type == ( - 1 ) ) BuyPointerList [ i ] = NULL ;
	    else BuyPointerList [ i ] = & ( SalesList[ i ] ) ;
	}
	
    }
    
}; // void InitTradeWithCharacter( void )

/* ----------------------------------------------------------------------
 * This is the menu, where you can select items for repair.
 *
 * NOTE:  THIS CODE IS CURRENTLY NOT IN USE, BECAUSE WE HAVE A GENERAL
 *        SHOP INTERFACE SIMILAR TO THE CHEST INTERFACE FOR THIS PURPOSE.
 *
 * ---------------------------------------------------------------------- */
void
Repair_Items( void )
{
#define BASIC_ITEMS_NUMBER 10
    item* Repair_Pointer_List[ MAX_ITEMS_IN_INVENTORY + 10 ];  // the inventory plus 7 slots or so
    int Pointer_Index;
    int i;
    // int InMenuPosition = 0;
    // int MenuInListPosition = 0;
    int ItemSelected=0;
    //char DescriptionText[5000];
    char* MenuTexts[ 10 ];
    int NumberOfItemsInTuxRow = 0 ;
    item* TuxItemsList[ MAX_ITEMS_IN_INVENTORY ];
    shop_decision ShopOrder;
    MenuTexts[0]=_("Yes");
    MenuTexts[1]=_("No");
    MenuTexts[2]="";
    
    
    Activate_Conservative_Frame_Computation();
    
    while ( ItemSelected != (-1) )
    {
	Pointer_Index=0;
	
	//--------------------
	// First we clean out the new Repair_Pointer_List
	//
	for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
	{
	    Repair_Pointer_List[ i ] = NULL;
	}
	
	//--------------------
	// Now we start to fill the Repair_Pointer_List
	//
	if ( ( Me.weapon_item.current_duration < Me.weapon_item.max_duration ) && 
	     ( Me.weapon_item.type != ( -1 ) ) )
	{
	    Repair_Pointer_List [ Pointer_Index ] = & ( Me.weapon_item );
	    Pointer_Index ++;
	}
	if ( ( Me.drive_item.current_duration < Me.drive_item.max_duration ) &&
	     ( Me.drive_item.type != ( -1 ) ) )
	{
	    Repair_Pointer_List [ Pointer_Index ] = & ( Me.drive_item );
	    Pointer_Index ++;
	}
	if ( ( Me.armour_item.current_duration < Me.armour_item.max_duration ) &&
	     ( Me.armour_item.type != ( -1 ) ) )
	{
	    Repair_Pointer_List [ Pointer_Index ] = & ( Me.armour_item );
	    Pointer_Index ++;
	}
	if ( ( Me.shield_item.current_duration < Me.shield_item.max_duration ) &&
	     ( Me.shield_item.type != ( -1 ) ) )
	{
	    Repair_Pointer_List [ Pointer_Index ] = & ( Me.shield_item );
	    Pointer_Index ++;
	}
	if ( ( Me.special_item.current_duration < Me.special_item.max_duration ) &&
	     ( Me.special_item.type != ( -1 ) ) )
	{
	    Repair_Pointer_List [ Pointer_Index ] = & ( Me.special_item );
	    Pointer_Index ++;
	}
	
	for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
	{
	    if ( Me.Inventory [ i ].type == (-1) ) continue;
	    if ( Me.Inventory [ i ].max_duration == (-1) ) continue;
	    if ( Me.Inventory [ i ].current_duration < Me.Inventory [ i ] .max_duration ) 
	    {
		Repair_Pointer_List [ Pointer_Index ] = & ( Me.Inventory[ i ] );
		Pointer_Index ++;
	    }
	}
	
	if ( Pointer_Index == 0 )
	{
	    MenuTexts[0]=_(" BACK ");
	    MenuTexts[1]="";
	    DoMenuSelection ( _(" YOU DONT HAVE ANYTHING THAT WOULD NEED REPAIR ") , MenuTexts , 1 , -1 , NULL );
	    return;
	}
	
	//--------------------
	// Now here comes the new thing:  This will be a loop from now
	// on.  The buy and buy and buy until at one point we say 'BACK'
	//
	//ItemSelected = 0;
	
	NumberOfItemsInTuxRow = AssemblePointerListForItemShow ( &( TuxItemsList[0]), FALSE );
	
	ItemSelected = GreatShopInterface ( Pointer_Index,  Repair_Pointer_List, 
					    0 , 0 , &(ShopOrder) , 2 );
		
	if ( ItemSelected == (-1) ) ShopOrder . shop_command = DO_NOTHING ;
	
	switch ( ShopOrder . shop_command )
	{
	    case BUY_1_ITEM:
		TryToRepairItem( Repair_Pointer_List[ ShopOrder . item_selected ] );
		break;
	}
    }
    /*while ( ItemSelected != (-1) )
      {
      sprintf( DescriptionText , " I COULD REPAIR THESE ITEMS           YOUR GOLD:  %4ld" , Me.Gold );
      ItemSelected = DoEquippmentListSelection( DescriptionText , Repair_Pointer_List , PRICING_FOR_REPAIR );
      if ( ItemSelected != (-1) ) TryToRepairItem( Repair_Pointer_List[ ItemSelected ] ) ;
      }*/
    
}; // void Repair_Items( void )

/* ----------------------------------------------------------------------
 * This is the menu, where you can buy basic items.
 *
 * NOTE:  THIS CODE IS CURRENTLY NOT IN USE, BECAUSE WE HAVE A GENERAL
 *        SHOP INTERFACE SIMILAR TO THE CHEST INTERFACE FOR THIS PURPOSE.
 *
 * ---------------------------------------------------------------------- */
void
Identify_Items ( void )
{
#define BASIC_ITEMS_NUMBER 10
    item* Identify_Pointer_List[ MAX_ITEMS_IN_INVENTORY + 10 ];  // the inventory plus 7 slots or so
    int Pointer_Index=0;
    int i;
    // int InMenuPosition = 0;
    // int MenuInListPosition = 0;
    char DescriptionText[5000];
    int ItemSelected;
    char* MenuTexts[ 10 ];
    MenuTexts[0]=_("Yes");
    MenuTexts[1]=_("No");
    MenuTexts[2]="";
    
    //--------------------
    // First we clean out the new Identify_Pointer_List
    //
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
	Identify_Pointer_List[ i ] = NULL;
    }
    
    //--------------------
    // Now we start to fill the Identify_Pointer_List
    //
    if ( ( !Me.weapon_item.is_identified ) && 
	 ( Me.weapon_item.type != ( -1 ) ) )
    {
	Identify_Pointer_List [ Pointer_Index ] = & ( Me.weapon_item );
	Pointer_Index ++;
    }
    if ( ( !Me.drive_item.is_identified ) &&
	 ( Me.drive_item.type != ( -1 ) ) )
    {
	Identify_Pointer_List [ Pointer_Index ] = & ( Me.drive_item );
	Pointer_Index ++;
    }
    if ( ( !Me.armour_item.is_identified ) &&
	 ( Me.armour_item.type != ( -1 ) ) )
    {
	Identify_Pointer_List [ Pointer_Index ] = & ( Me.armour_item );
	Pointer_Index ++;
    }
    if ( ( !Me.shield_item.is_identified ) &&
	 ( Me.shield_item.type != ( -1 ) ) )
    {
	Identify_Pointer_List [ Pointer_Index ] = & ( Me.shield_item );
	Pointer_Index ++;
    }
    
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
	if ( Me.Inventory [ i ].type == (-1) ) continue;
	if ( Me.Inventory [ i ].is_identified ) continue;
	
	Identify_Pointer_List [ Pointer_Index ] = & ( Me.Inventory[ i ] );
	Pointer_Index ++;
    }
    
    if ( Pointer_Index == 0 )
    {
	MenuTexts[0]=_(" BACK ");
	MenuTexts[1]="";
	DoMenuSelection ( _(" YOU DONT HAVE ANYTHING THAT WOULD NEED TO BE IDENTIFIED!") , MenuTexts , 1 , -1 , NULL );
	return;
    }
    
    //--------------------
    // Now here comes the new thing:  This will be a loop from now
    // on.  The buy and buy and buy until at one point we say 'BACK'
    //
    ItemSelected = 0;
    
    while ( ItemSelected != (-1) )
    {
	sprintf( DescriptionText , _(" I COULD IDENTIFY THESE ITEMS             YOUR GOLD:  %4ld") , Me.Gold );
	ItemSelected = DoEquippmentListSelection( DescriptionText , Identify_Pointer_List , PRICING_FOR_IDENTIFY );
	if ( ItemSelected != (-1) ) TryToIdentifyItem( Identify_Pointer_List[ ItemSelected ] ) ;
    }
    
}; // void Identify_Items( void )


/* ----------------------------------------------------------------------
 * This is the menu, where you can sell inventory items.
 *
 * NOTE:  THIS CODE IS CURRENTLY NOT IN USE, BECAUSE WE HAVE A GENERAL
 *        SHOP INTERFACE SIMILAR TO THE CHEST INTERFACE FOR THIS PURPOSE.
 *
 * ---------------------------------------------------------------------- */
void
Sell_Items( int ForHealer )
{
#define BASIC_ITEMS_NUMBER 10
    item* Sell_Pointer_List[ MAX_ITEMS_IN_INVENTORY ];
    int Pointer_Index=0;
    int i;
    int ItemSelected;
    //  int InMenuPosition = 0;
    //  int MenuInListPosition = 0;
    char DescriptionText[5000];
    char* MenuTexts[ 10 ];
    MenuTexts[0]=_("Yes");
    MenuTexts[1]=_("No");
    MenuTexts[2]="";
    
    //--------------------
    // First we clean out the new Sell_Pointer_List
    //
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
	Sell_Pointer_List[ i ] = NULL;
    }
    
    //--------------------
    // Now we start to fill the Sell_Pointer_List
    //
    for ( i = 0 ; i < MAX_ITEMS_IN_INVENTORY ; i ++ )
    {
	if ( Me.Inventory [ i ].type == (-1) ) continue;
	else
	{
	    //--------------------
	    // Now depending on whether we sell to the healer or to
	    // the weaponsmith, we can either sell one thing or the
	    // other
	    //
	    if ( ( ForHealer ) &&  ! ItemMap [ Me.Inventory[ i ].type ].item_can_be_applied_in_combat ) continue;
	    if ( ! ( ForHealer ) &&  ItemMap [ Me.Inventory[ i ].type ].item_can_be_applied_in_combat ) continue;
	    Sell_Pointer_List [ Pointer_Index ] = & ( Me.Inventory[ i ] );
	    Pointer_Index ++;
	}
    }
    
    if ( Pointer_Index == 0 )
    {
	MenuTexts[0]=_(" BACK ");
	MenuTexts[1]="";
	DoMenuSelection ( _(" YOU DONT HAVE ANYTHING IN INVENTORY (I.E. NOT WORN), THAT COULD BE SOLD. "), 
			  MenuTexts, 1 , -1 , NULL );
	return;
    }
    
    //--------------------
    // Now here comes the new thing:  This will be a loop from now
    // on.  The buy and buy and buy until at one point we say 'BACK'
    //
    ItemSelected = 0;
    
    while ( ItemSelected != (-1) )
    {
	sprintf( DescriptionText , _(" I WOULD BUY FROM YOU THESE ITEMS        YOUR GOLD:  %4ld"), Me.Gold );
	ItemSelected = DoEquippmentListSelection( DescriptionText , Sell_Pointer_List , PRICING_FOR_SELL );
	if ( ItemSelected != (-1) ) TryToSellItem( Sell_Pointer_List[ ItemSelected ] , TRUE , 1 ) ;
    }
    
}; // void Sell_Items( int ForHealer )

/* ----------------------------------------------------------------------
 * This function does all the buying/selling interaction with the 
 * weaponsmith Mr. Stone.
 *
 * NOTE:  THIS CODE IS CURRENTLY NOT IN USE, BECAUSE WE HAVE A GENERAL
 *        SHOP INTERFACE SIMILAR TO THE CHEST INTERFACE FOR THIS PURPOSE.
 *
 * ---------------------------------------------------------------------- */
void
BuySellMenu ( void )
{
    enum
	{ 
	    BUY_BASIC_ITEMS=1, 
	    BUY_PREMIUM_ITEMS, 
	    SELL_ITEMS, 
	    REPAIR_ITEMS,
	    IDENTIFY_ITEMS,
	    LEAVE_BUYSELLMENU
	};
    
    int Weiter = 0;
    int MenuPosition=1;
    char* MenuTexts[10];
    
    Me.status=MENU;
    
    DebugPrintf (2, "\nvoid BuySellMenu(void): real function call confirmed."); 
    
    // Prevent distortion of framerate by the delay coming from 
    // the time spend in the menu.
    Activate_Conservative_Frame_Computation();
    while ( EscapePressed() );
    
    while (!Weiter)
    {
	MenuTexts[0]=_("Buy Basic Items");
	MenuTexts[1]=_("Buy Premium Items");
	MenuTexts[2]=_("Sell Items");
	MenuTexts[3]=_("Repair Items");
	MenuTexts[5]=_("Leave the Sales Representative");
	MenuTexts[4]=_("Identify Items");
	MenuTexts[8]="";
	
	// MenuPosition = DoMenuSelection( "" , MenuTexts , -1 , SHOP_BACKGROUND_IMAGE , NULL );
	MenuPosition = DoMenuSelection( "" , MenuTexts , -1 , SHOP_BACKGROUND_IMAGE_CODE , Message_BFont );
	
	switch (MenuPosition) 
	{
	    case (-1):
		Weiter=!Weiter;
		break;
	    case BUY_BASIC_ITEMS:
		while (EnterPressed() || SpacePressed() );
		InitTradeWithCharacter( FALSE );
		break;
	    case BUY_PREMIUM_ITEMS:
		while (EnterPressed() || SpacePressed() );
		InitTradeWithCharacter( FALSE );
		break;
	    case SELL_ITEMS:
		while (EnterPressed() || SpacePressed() );
		Sell_Items( FALSE );
		break;
	    case REPAIR_ITEMS:
		while (EnterPressed() || SpacePressed() );
		Repair_Items();
		break;
	    case IDENTIFY_ITEMS:
		while (EnterPressed() || SpacePressed() );
		Identify_Items();
		break;
	    case LEAVE_BUYSELLMENU:
		Weiter = !Weiter;
		break;
	    default: 
		break;
	} 
    }
    
    ClearGraphMem();
    // Since we've faded out the whole scren, it can't hurt
    // to have the top status bar redrawn...
    Me.status=MOBILE;
    
    return;
}; // void BuySellMenu ( void )

/* ----------------------------------------------------------------------
 * When the Tux opens a chest map tile, then there should be an interface
 * where the Tux can put in stuff and take out stuff from the chest, 
 * which is exactly what this function is supposed to do.
 * ---------------------------------------------------------------------- */
void
EnterChest ( moderately_finepoint pos )
{
    int ItemSelected = 0 ;
    int NumberOfItemsInTuxRow = 0 ;
    int NumberOfItemsInChest = 0 ;
    item* Buy_Pointer_List[ MAX_ITEMS_IN_INVENTORY ];
    item* TuxItemsList[ MAX_ITEMS_IN_INVENTORY ];
    shop_decision ShopOrder;
    
    Activate_Conservative_Frame_Computation();
    
    play_open_chest_sound();
    
    while ( ItemSelected != (-1) )
    {
	
	NumberOfItemsInTuxRow = AssemblePointerListForItemShow ( &( TuxItemsList[0]), FALSE );
	NumberOfItemsInChest = AssemblePointerListForChestShow ( &( Buy_Pointer_List[0]), pos );
	
	ItemSelected = GreatShopInterface ( NumberOfItemsInChest , Buy_Pointer_List , 
					    NumberOfItemsInTuxRow , TuxItemsList , &(ShopOrder) , TRUE );
	
	if ( ItemSelected == (-1) ) ShopOrder . shop_command = DO_NOTHING ;
	
	switch ( ShopOrder . shop_command )
	{
	    case BUY_1_ITEM:
		// TryToBuyItem( BuyPointerList[ ShopOrder . item_selected ] , FALSE , ShopOrder . number_selected ) ;
		TryToTakeItem( Buy_Pointer_List[ ShopOrder . item_selected ] , ShopOrder . number_selected ) ;
		break;
	    case BUY_10_ITEMS:
		TryToTakeItem( Buy_Pointer_List[ ShopOrder . item_selected ] , 10 ) ;
		break;
	    case BUY_100_ITEMS:
		TryToTakeItem( Buy_Pointer_List[ ShopOrder . item_selected ] , 100 ) ;
		break;
	    case SELL_1_ITEM:
		// TryToPutItem( TuxItemsList[ ShopOrder . item_selected ] , 1 , pos ) ;
		TryToPutItem( TuxItemsList[ ShopOrder . item_selected ] , ShopOrder . number_selected , pos ) ;
		break;
	    case SELL_10_ITEMS:
		TryToPutItem( TuxItemsList[ ShopOrder . item_selected ] , 10 , pos ) ;
		break;
	    case SELL_100_ITEMS:
		TryToPutItem( TuxItemsList[ ShopOrder . item_selected ] , 100 , pos ) ;
		break;
	    default:
		
		break;
	};
    }
    
}; // void EnterChest (void)


#undef _shop_c

