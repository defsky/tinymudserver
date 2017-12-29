/*

 tinymudserver - an example MUD server

 Author:  Nick Gammon 
          http://www.gammon.com.au/ 

(C) Copyright Nick Gammon 2004. Permission to copy, use, modify, sell and
distribute this software is granted provided this copyright notice appears
in all copies. This software is provided "as is" without express or implied
warranty, and with no claim as to its suitability for any purpose.
 
*/

// standard library includes ...

#include <stdexcept>
#include <fstream>
#include <iostream>

using namespace std; 

#include "utils.h"
#include "player.h"
#include "globals.h"

void PlayerEnteredGame (tPlayer * p, const string & message)
{
  p->connstate = ePlaying;    // now normal player
  p->prompt = PROMPT;         // default prompt
  *p << messagemap ["welcome"] << p->playername << "\n\n\r"; // greet them
  *p << message;
  *p << messagemap ["motd"];  // message of the day
  p->DoCommand ("look");     // new player looks around

  // tell other players
  SendToAll (
    //"Player " + p->playername + " has joined the game from " + p->GetAddress () + ".\n", 
    p->playername + messagemap["server_player_joined"], 
    p);
  
  // log it
  cout << "Player " << p->playername << " has joined the game." << endl;
} // end of PlayerEnteredGame

void ProcessPlayerName (tPlayer * p, istream & sArgs)
{
  string playername;
  sArgs >> playername;

  /* name can't be blank */
  if (playername.empty ())
    //throw runtime_error ("Name cannot be blank.");
    throw runtime_error (messagemap["error_name_blank"]);
  
  /* don't allow two of the same name */
  if (FindPlayer (playername))
    //throw runtime_error (playername + " is already connected.");
    throw runtime_error (playername + messagemap["error_name_online"]);

  if (playername.find_first_not_of (valid_player_name) != string::npos)
    //throw runtime_error ("That player name contains disallowed characters.");
    throw runtime_error (messagemap["error_name_invalid"]);
        
  if (tolower (playername) == "new")
    {
    p->connstate = eAwaitingNewName;
    //p->prompt = "Please choose a name for your new character ... ";
    p->prompt = messagemap["prompt_new_name"];
    }   // end of new player
  else
    {   // old player
  
    p->playername = tocapitals (playername);
    p->Load ();   // load player so we know the password etc.
    
    p->connstate = eAwaitingPassword;
    //p->prompt = "Enter your password ... ";
    p->prompt = messagemap["prompt_password"];
    p->badPasswordCount = 0;
    } // end of old player
        
} /* end of ProcessPlayerName */

void ProcessNewPlayerName (tPlayer * p, istream & sArgs)
{
  string playername;
  sArgs >> playername;
  
  /* name can't be blank */
  if (playername.empty ())
    //throw runtime_error ("Name cannot be blank.");
    throw runtime_error (messagemap["error_name_blank"]);

  if (playername.find_first_not_of (valid_player_name) != string::npos)
    //throw runtime_error ("That player name contains disallowed characters.");
    throw runtime_error (messagemap["error_name_invalid"]);
        
  // check for bad names here (from list in control file)
  if (badnameset.find (playername) != badnameset.end ())
    //throw runtime_error ("That name is not permitted.");
    throw runtime_error (messagemap["error_name_banned"]);
    
  ifstream f ((PLAYER_DIR + tocapitals (playername) + PLAYER_EXT).c_str (), ios::in);
  if (f || FindPlayer (playername))  // player file on disk, or playing without saving yet
    //throw runtime_error ("That player already exists, please choose another name.");
    throw runtime_error (messagemap["error_name_exist"]);
  
  p->playername = tocapitals (playername);
  
  p->connstate = eAwaitingNewPassword;
  //p->prompt = "Choose a password for " + p->playername + " ... ";  
  p->prompt = messagemap["prompt_new_password"];  
  p->badPasswordCount = 0;
    
} /* end of ProcessNewPlayerName */

void ProcessNewPassword (tPlayer * p, istream & sArgs)
{
   string password;
   sArgs >> password;
  
  /* password can't be blank */
  if (password.empty ())
    //throw runtime_error ("Password cannot be blank.");
    throw runtime_error (messagemap["error_password_blank"]);
  
  p->password = password;
  p->connstate = eConfirmPassword;
  //p->prompt = "Re-enter password to confirm it ... ";
  p->prompt = messagemap["prompt_password_reenter"];
    
} /* end of ProcessNewPassword */

void ProcessConfirmPassword (tPlayer * p, istream & sArgs)
{
   string password;
   sArgs >> password;
  
  // password must agree
  if (password != p->password)
    {
    p->connstate = eAwaitingNewPassword;
    //p->prompt = "Choose a password for " + p->playername + " ... ";
    p->prompt = messagemap["prompt_password"];
    //throw runtime_error ("Password and confirmation do not agree.");
    throw runtime_error (messagemap["error_password_confirm_failed"]);
    }
  
  // that player might have been created while we were choosing a password, so check again
  ifstream f ((PLAYER_DIR + tocapitals (password) + PLAYER_EXT).c_str (), ios::in);
  if (f || FindPlayer (password))  // player file on disk, or playing without saving yet
    {
    p->connstate = eAwaitingNewName;
    p->prompt = messagemap["prompt_new_name"];  // re-prompt for name
    throw runtime_error (messagemap["error_name_exist"]);
    }
  
  // New player now in the game
  PlayerEnteredGame (p, messagemap ["new_player"]);
         
} /* end of ProcessNewPassword */

void ProcessPlayerPassword (tPlayer * p, istream & sArgs)
{
  try
    {
    string password;
    sArgs >> password;

    /* password can't be blank */
    if (password.empty ())
      //throw runtime_error ("Password cannot be blank.");
      throw runtime_error (messagemap["error_password_blank"]);
        
    if (password != p->password)
      //throw runtime_error ("That password is incorrect.");
      throw runtime_error (messagemap["error_password_incorrect"]);

    // check for "blocked" flag on this player
    if (p->HaveFlag ("blocked"))
      {
      p->ClosePlayer ();
      //p->prompt = "Goodbye.\n";
      p->prompt = messagemap["prompt_flag_blocked"];
      //throw runtime_error ("You are not permitted to connect.");
      throw runtime_error (messagemap["server_flag_blocked"]);
      }

    if (p->HaveFlag ("need_new_name"))
    {
      p->connstate = eAwaitingNewName;
      p->prompt = messagemap["prompt_new_surname"];  // re-prompt for name
      throw runtime_error (messagemap["error_surname_cleared"]);
    }
      
    // OK, they're in!
    PlayerEnteredGame (p, messagemap ["existing_player"]);
 
    } // end of try block
    
  // detect too many password attempts
  catch (runtime_error & e)
    {
    if (++p->badPasswordCount >= MAX_PASSWORD_ATTEMPTS)
      {
      //*p << "Too many attempts to guess the password!\n";
      *p << messagemap["server_password_attempt_exceeded"];
      p->Init ();
      }
      throw;
    }
    
} /* end of ProcessPlayerPassword */

void ProcessNewSurname (tPlayer * p, istream & sArgs)  
{
  string playername;
  sArgs >> playername;
  
  /* name can't be blank */
  if (playername.empty ())
    //throw runtime_error ("Name cannot be blank.");
    throw runtime_error (messagemap["error_name_blank"]);

  if (playername.find_first_not_of (valid_player_name) != string::npos)
    //throw runtime_error ("That player name contains disallowed characters.");
    throw runtime_error (messagemap["error_name_invalid"]);
        
  // check for bad names here (from list in control file)
  if (badnameset.find (playername) != badnameset.end ())
    //throw runtime_error ("That name is not permitted.");
    throw runtime_error (messagemap["error_name_banned"]);
    
  ifstream f ((PLAYER_DIR + tocapitals (playername) + PLAYER_EXT).c_str (), ios::in);
  if (f || FindPlayer (playername))  // player file on disk, or playing without saving yet
    //throw runtime_error ("That player already exists, please choose another name.");
    throw runtime_error (messagemap["error_name_exist"]);
  
  p->playername = tocapitals (playername);
  
  p->connstate = eAwaitingNewPassword;
  //p->prompt = "Choose a password for " + p->playername + " ... ";  
  p->prompt = messagemap["prompt_new_password"];  
  p->badPasswordCount = 0;
} /* end of ProcessNewSurname */

void LoadStates ()
{

// connection states
  statemap [eAwaitingName]        = ProcessPlayerName;    // existing player
  statemap [eAwaitingPassword]    = ProcessPlayerPassword;

  statemap [eAwaitingNewName]     = ProcessNewPlayerName; // new player
  statemap [eAwaitingNewPassword] = ProcessNewPassword;
  statemap [eConfirmPassword]     = ProcessConfirmPassword;
  statemap [eAwaitingNewName]     = ProcessNewSurname;
  statemap [ePlaying]             = ProcessCommand;   // playing

} // end of LoadStates

