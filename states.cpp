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
    p->badPasswordCount = 0;

    if (p->HaveFlag ("need_new_name"))
    {
      p->connstate = eAwaitingSurname;
      p->prompt = messagemap["prompt_new_surname"];  // re-prompt for surname
      throw runtime_error (messagemap["error_surname_cleared"]);
    }

  p->connstate = ePlaying;    // now normal player
  p->prompt = PROMPT;         // default prompt
  
// greet them
  *p << messagemap ["welcome"] << p->GetFullname() << "\n\n\r"; 
  *p << message;
  *p << messagemap ["motd"];  // message of the day
  p->DoCommand ("look");     // new player looks around

  // tell other players
  SendToAll (
    p->GetFullname() + messagemap["server_player_joined"], 
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
    throw runtime_error (messagemap["error_name_blank"]);
  
  /* don't allow two of the same name */
  if (FindPlayer (playername))
    throw runtime_error (playername + messagemap["error_name_online"]);

  if (playername.find_first_not_of (valid_player_name) != string::npos)
    throw runtime_error (messagemap["error_name_invalid"]);
        
  if (tolower (playername) == "new")
    {
    p->connstate = eAwaitingNewName;
    p->prompt = messagemap["prompt_new_name"];
    }   // end of new player
  else
    {   // old player
  
    p->playername = tocapitals (playername);
    p->Load ();   // load player so we know the password etc.
    
    p->connstate = eAwaitingPassword;
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
    throw runtime_error (messagemap["error_name_blank"]);

  if (playername.find_first_not_of (valid_player_name) != string::npos)
    throw runtime_error (messagemap["error_name_invalid"]);
        
  // check for bad names here (from list in control file)
  if (badnameset.find (playername) != badnameset.end ())
    throw runtime_error (messagemap["error_name_banned"]);
    
  ifstream f ((PLAYER_DIR + tocapitals (playername) + PLAYER_EXT).c_str (), ios::in);
  if (f || FindPlayer (playername))  // player file on disk, or playing without saving yet
    throw runtime_error (messagemap["error_name_exist"]);
  
  p->playername = tocapitals (playername);
  
  p->connstate = eAwaitingNewSurname;
  p->prompt = messagemap["prompt_new_surname"];  
    
} /* end of ProcessNewPlayerName */

void ProcessNewSurname (tPlayer * p, istream & sArgs)  
{
  string surname;
  sArgs >> surname;

  surname = translate_to_charset("utf-8","gb2312",surname);
  
  /* surname can't be blank */
  if (surname.empty ())
    throw runtime_error (messagemap["error_surname_blank"]);

  if (surname.find_first_of (valid_player_name) != string::npos)
    throw runtime_error (messagemap["error_surname_invalid"]);
        
  // check for bad surnames here (from list in control file)
  if (badnameset.find (surname) != badnameset.end ())
    throw runtime_error (messagemap["error_surname_invalid"]);
    
  p->surname = surname;

  p->connstate = eAwaitingNewPassword;
  p->prompt = messagemap["prompt_new_password"];  
  p->badPasswordCount = 0;
} /* end of ProcessNewSurname */

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

    // OK, they're in!
    PlayerEnteredGame (p, messagemap ["existing_player"]);
 
    } // end of try block
    
  // detect too many password attempts
  catch (runtime_error & e)
    {
    if (++p->badPasswordCount >= MAX_PASSWORD_ATTEMPTS)
      {
      *p << messagemap["server_password_attempt_exceeded"];
      p->Init ();
      }
      throw;
    }
    
} /* end of ProcessPlayerPassword */

void ProcessSurname (tPlayer * p, istream & sArgs)  
{
  string surname;
  sArgs >> surname;

  surname = translate_to_charset("utf-8","gb2312",surname);
  
  /* surname can't be blank */
  if (surname.empty ())
    throw runtime_error (messagemap["error_surname_blank"]);

  if (surname.find_first_of (valid_player_name) != string::npos)
    throw runtime_error (messagemap["error_surname_invalid"]);
        
  // check for bad surnames here (from list in control file)
  if (badnameset.find (surname) != badnameset.end ())
    throw runtime_error (messagemap["error_surname_invalid"]);
    
  p->surname = surname;
  p->flags.erase("need_new_name");
  p->Save();

  // OK, they're in!
  PlayerEnteredGame (p, messagemap ["server_existing_player_login"]);
} /* end of ProcessNewSurname */

void LoadStates ()
{

// connection states
  statemap [eAwaitingName]        = ProcessPlayerName;    // existing player
  statemap [eAwaitingPassword]    = ProcessPlayerPassword;

  statemap [eAwaitingNewName]     = ProcessNewPlayerName; // new player
  statemap [eAwaitingNewSurname]  = ProcessNewSurname; // new player
  statemap [eAwaitingNewPassword] = ProcessNewPassword;

  statemap [eConfirmPassword]     = ProcessConfirmPassword;
  statemap [eAwaitingSurname]     = ProcessSurname;
  statemap [ePlaying]             = ProcessCommand;   // playing

} // end of LoadStates

