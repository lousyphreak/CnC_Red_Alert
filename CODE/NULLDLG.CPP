/*
**Command & Conquer Red Alert(tm)
**Copyright 2025 Electronic Arts Inc.
**
**This program is free software: you can redistribute it and/or modify
**it under the terms of the GNU General Public License as published by
**the Free Software Foundation, either version 3 of the License, or
**(at your option) any later version.
**
**This program is distributed in the hope that it will be useful,
**but WITHOUT ANY WARRANTY; without even the implied warranty of
**MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**GNU General Public License for more details.
**
**You should have received a copy of the GNU General Public License
**along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header: /counterstrike/NULLDLG.CPP 14    3/17/97 1:05a Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : NULLDLG.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Bill Randolph                                                *
 *                                                                                             *
 *                  Last Update : April 2, 2026                                                *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Overview:                                                                                   *
 *   The original serial/modem setup code has been removed from the SDL3 port.                 *
 *   The surviving shared pieces are the skirmish scenario picker and the local                *
 *   scenario lookup helper used by the remaining multiplayer paths.                           *
 *---------------------------------------------------------------------------------------------*/

#include "FUNCTION.H"

extern char const *EngMisStr[];
extern bool Is_Mission_Counterstrike(char *file_name);

#ifdef FIXIT_CSII
extern bool Is_Mission_Aftermath(char *file_name);
#endif

#define PCOLOR_BROWN PCOLOR_GREY

/***********************************************************************************************
 * Com_Scenario_Dialog -- Skirmish scenario selection dialog                                   *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      skirmish  must be true; the serial/modem path was removed                              *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      true = success, false = cancel                                                         *
 ***********************************************************************************************/
int Com_Scenario_Dialog(bool skirmish)
{
if (!skirmish) {
return(false);
}

int d_dialog_w = 320 * RESFACTOR;
int d_dialog_h = 200 * RESFACTOR;
int d_dialog_x = ((320 * RESFACTOR - d_dialog_w) / 2);
int d_dialog_y = ((200 * RESFACTOR - d_dialog_h) / 2);
int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);

int d_txt6_h = 6 * RESFACTOR + 1;
int d_margin1 = 5 * RESFACTOR;
int d_margin2 = 7 * RESFACTOR;

int d_name_w = 70 * RESFACTOR;
int d_name_h = 9 * RESFACTOR;
int d_name_x = d_dialog_x + (d_dialog_w / 4) - (d_name_w / 2);
int d_name_y = d_dialog_y + d_margin2 + d_txt6_h + 1 * RESFACTOR;

int d_house_w = 60 * RESFACTOR;
int d_house_h = (8 * 5 * RESFACTOR);
int d_house_x = d_dialog_cx - (d_house_w / 2);
int d_house_y = d_name_y;

int d_color_w = 10 * RESFACTOR;
int d_color_h = 9 * RESFACTOR;
int d_color_y = d_name_y;
int d_color_x = d_dialog_x + ((d_dialog_w / 4) * 3) - (d_color_w * 3);

int d_playerlist_w = 118 * RESFACTOR;
int d_playerlist_x = d_dialog_x + d_margin1 + d_margin1 + 5 * RESFACTOR;

int d_scenariolist_w = 162 * RESFACTOR;
int d_scenariolist_h = ((6 * 6 * RESFACTOR) + 3 * RESFACTOR) * 2;
int d_scenariolist_x = d_dialog_x + (d_dialog_w - d_scenariolist_w) / 2;
int d_scenariolist_y = d_color_y + d_color_h + d_margin2 + 2 * RESFACTOR;

int d_count_w = 25 * RESFACTOR;
int d_count_h = d_txt6_h;
int d_count_x = d_playerlist_x + (d_playerlist_w / 2) + 20 * RESFACTOR;
int d_count_y = d_scenariolist_y + d_scenariolist_h + d_margin1 - 2 * RESFACTOR;

int d_level_w = 25 * RESFACTOR;
int d_level_h = d_txt6_h;
int d_level_x = d_count_x;
int d_level_y = d_count_y + d_count_h;

int d_credits_w = 25 * RESFACTOR;
int d_credits_h = d_txt6_h;
int d_credits_x = d_count_x;
int d_credits_y = d_level_y + d_level_h;

int d_aiplayers_w = 25 * RESFACTOR;
int d_aiplayers_h = d_txt6_h;
int d_aiplayers_x = d_count_x;
int d_aiplayers_y = d_credits_y + d_credits_h;

int d_options_w = 106 * RESFACTOR;
int d_options_h = (4 * 6 * RESFACTOR) + 4 * RESFACTOR;
int d_options_x = d_dialog_x + d_dialog_w - 149 * RESFACTOR;
int d_options_y = d_scenariolist_y + d_scenariolist_h + d_margin1 - 2 * RESFACTOR;

int d_ok_w = 45 * RESFACTOR;
int d_ok_h = 9 * RESFACTOR;
int d_ok_x = d_dialog_x + (d_dialog_w / 6) - (d_ok_w / 2);
int d_ok_y = d_dialog_y + d_dialog_h - d_ok_h - d_margin1 - RESFACTOR * 6;

int d_cancel_w = 45 * RESFACTOR;
int d_cancel_h = 9 * RESFACTOR;
int d_cancel_x = d_dialog_cx - (d_cancel_w / 2);
int d_cancel_y = d_ok_y;

enum {
BUTTON_NAME = 100,
BUTTON_HOUSE,
BUTTON_CREDITS,
BUTTON_AIPLAYERS,
BUTTON_OPTIONS,
BUTTON_SCENARIOLIST,
BUTTON_COUNT,
BUTTON_LEVEL,
BUTTON_OK,
BUTTON_CANCEL,
BUTTON_DIFFICULTY,
};

typedef enum {
REDRAW_NONE = 0,
REDRAW_PARMS,
REDRAW_COLORS,
REDRAW_BUTTONS,
REDRAW_BACKGROUND,
REDRAW_ALL = REDRAW_BACKGROUND
} RedrawType;

static bool first_time = true;
RedrawType display = REDRAW_ALL;
bool process = true;
bool changed = false;
bool rc = false;
KeyNumType input;
int i;
char *item;
char namebuf[MPLAYER_NAME_MAX] = {0};
char staticcountbuff[35];
char staticlevelbuff[35];
char staticcreditsbuff[35];
char staticaibuff[35];
int optiontabs[] = {8};
int cbox_x[] = {
d_color_x,
d_color_x + d_color_w,
d_color_x + (d_color_w * 2),
d_color_x + (d_color_w * 3),
d_color_x + (d_color_w * 4),
d_color_x + (d_color_w * 5),
d_color_x + (d_color_w * 6),
d_color_x + (d_color_w * 7)
};

RemapControlType *scheme = GadgetClass::Get_Color_Scheme();
GadgetClass *commands;

Set_Logic_Page(SeenBuff);

EditClass name_edt(BUTTON_NAME, namebuf, MPLAYER_NAME_MAX, TPF_TEXT,
d_name_x, d_name_y, d_name_w, d_name_h, EditClass::ALPHANUMERIC);
char housetext[25] = "";
Fancy_Text_Print("", 0, 0, 0, 0, TPF_TEXT);
DropListClass housebtn(BUTTON_HOUSE, housetext, sizeof(housetext), TPF_TEXT,
d_house_x, d_house_y, d_house_w, d_house_h,
MFCD::Retrieve("BTN-UP.SHP"), MFCD::Retrieve("BTN-DN.SHP"));
ListClass scenariolist(BUTTON_SCENARIOLIST, d_scenariolist_x, d_scenariolist_y,
d_scenariolist_w, d_scenariolist_h, TPF_TEXT,
MFCD::Retrieve("BTN-UP.SHP"), MFCD::Retrieve("BTN-DN.SHP"));
GaugeClass countgauge(BUTTON_COUNT, d_count_x, d_count_y, d_count_w, d_count_h);
StaticButtonClass staticcount(0, "     ", TPF_TEXT,
d_count_x + d_count_w + 3 * RESFACTOR, d_count_y);
GaugeClass levelgauge(BUTTON_LEVEL, d_level_x, d_level_y, d_level_w, d_level_h);
StaticButtonClass staticlevel(0, "     ", TPF_TEXT,
d_level_x + d_level_w + 3 * RESFACTOR, d_level_y);
GaugeClass creditsgauge(BUTTON_CREDITS, d_credits_x, d_credits_y, d_credits_w, d_credits_h);
StaticButtonClass staticcredits(0, "         ", TPF_TEXT,
d_credits_x + d_credits_w + 3 * RESFACTOR, d_credits_y);
GaugeClass aiplayersgauge(BUTTON_AIPLAYERS, d_aiplayers_x, d_aiplayers_y,
d_aiplayers_w, d_aiplayers_h);
StaticButtonClass staticai(0, "     ", TPF_TEXT,
d_aiplayers_x + d_aiplayers_w + 3 * RESFACTOR, d_aiplayers_y);
CheckListClass optionlist(BUTTON_OPTIONS, d_options_x, d_options_y, d_options_w,
d_options_h, TPF_TEXT, MFCD::Retrieve("BTN-UP.SHP"), MFCD::Retrieve("BTN-DN.SHP"));
TextButtonClass okbtn(BUTTON_OK, TXT_OK, TPF_BUTTON,
d_ok_x, d_ok_y, d_ok_w, d_ok_h);
TextButtonClass cancelbtn(BUTTON_CANCEL, TXT_CANCEL, TPF_BUTTON,
d_cancel_x, d_cancel_y, d_cancel_w, d_cancel_h);
SliderClass difficulty(BUTTON_DIFFICULTY, d_name_x,
optionlist.Y + optionlist.Height + d_margin1 + d_margin1,
d_dialog_w - (d_name_x - d_dialog_x) * 2, 8 * RESFACTOR, true);

if (Rule.IsFineDifficulty) {
difficulty.Set_Maximum(5);
difficulty.Set_Value(2);
} else {
difficulty.Set_Maximum(3);
difficulty.Set_Value(1);
}

commands = &name_edt;
staticcount.Add_Tail(*commands);
staticcredits.Add_Tail(*commands);
staticai.Add_Tail(*commands);
staticlevel.Add_Tail(*commands);
difficulty.Add_Tail(*commands);
scenariolist.Add_Tail(*commands);
countgauge.Add_Tail(*commands);
levelgauge.Add_Tail(*commands);
creditsgauge.Add_Tail(*commands);
aiplayersgauge.Add_Tail(*commands);
optionlist.Add_Tail(*commands);
okbtn.Add_Tail(*commands);
cancelbtn.Add_Tail(*commands);
housebtn.Add_Tail(*commands);

Session.ColorIdx = Session.PrefColor;
strcpy(namebuf, Session.Handle);
name_edt.Set_Text(namebuf, MPLAYER_NAME_MAX);
name_edt.Set_Color(&ColorRemaps[(Session.ColorIdx == PCOLOR_DIALOG_BLUE) ?
PCOLOR_REALLY_BLUE : Session.ColorIdx]);

for (HousesType house = HOUSE_USSR; house <= HOUSE_FRANCE; house++) {
housebtn.Add_Item(Text_String(HouseTypeClass::As_Reference(house).Full_Name()));
}
housebtn.Set_Selected_Index(Session.House - HOUSE_USSR);
housebtn.Set_Read_Only(true);

Special.IsCaptureTheFlag = Rule.IsMPCaptureTheFlag;
if (first_time) {
Session.Options.Credits = Rule.MPDefaultMoney;
Session.Options.Bases = Rule.IsMPBasesOn;
Session.Options.Tiberium = Rule.IsMPTiberiumGrow;
Session.Options.Goodies = Rule.IsMPCrates;
Session.Options.AIPlayers = 1;
Special.IsShadowGrow = Rule.IsMPShadowGrow;
Session.Options.UnitCount =
(SessionClass::CountMax[Session.Options.Bases] + SessionClass::CountMin[Session.Options.Bases]) / 2;
first_time = false;
}

optionlist.Set_Tabs(optiontabs);
optionlist.Set_Read_Only(0);
optionlist.Add_Item(Text_String(TXT_BASES));
optionlist.Add_Item(Text_String(TXT_ORE_SPREADS));
optionlist.Add_Item(Text_String(TXT_CRATES));
optionlist.Add_Item(Text_String(TXT_SHADOW_REGROWS));
optionlist.Check_Item(0, Session.Options.Bases);
optionlist.Check_Item(1, Session.Options.Tiberium);
optionlist.Check_Item(2, Session.Options.Goodies);
optionlist.Check_Item(3, Special.IsShadowGrow);

countgauge.Set_Maximum(SessionClass::CountMax[Session.Options.Bases] - SessionClass::CountMin[Session.Options.Bases]);
countgauge.Set_Value(Session.Options.UnitCount - SessionClass::CountMin[Session.Options.Bases]);
levelgauge.Set_Maximum(MPLAYER_BUILD_LEVEL_MAX - 1);
levelgauge.Set_Value(BuildLevel - 1);
creditsgauge.Set_Maximum(Rule.MPMaxMoney);
creditsgauge.Set_Value(Session.Options.Credits);

int maxp = Rule.MaxPlayers - 2;
aiplayersgauge.Set_Maximum(maxp);
if (Session.Options.AIPlayers > 7) {
Session.Options.AIPlayers = 7;
}
Session.Options.AIPlayers = max(Session.Options.AIPlayers, 1);
aiplayersgauge.Set_Value(Session.Options.AIPlayers - 1);

Special.IsTGrowth = Rule.IsTGrowth = Special.IsTSpread = Rule.IsTSpread = Session.Options.Tiberium;
Clear_Vector(&Session.Players);

for (i = 0; i < Session.Scenarios.Count(); i++) {
bool found = false;
for (int j = 0; EngMisStr[j] != NULL; j++) {
if (!strcmp(Session.Scenarios[i]->Description(), EngMisStr[j])) {
#ifdef FIXIT_CSII
if (!((Is_Mission_Counterstrike((char *)(Session.Scenarios[i]->Get_Filename())) && !Is_Counterstrike_Installed()) ||
(Is_Mission_Aftermath((char *)(Session.Scenarios[i]->Get_Filename())) && !Is_Aftermath_Installed())))
#endif
#if defined(GERMAN) || defined(FRENCH)
scenariolist.Add_Item(EngMisStr[j + 1]);
#else
scenariolist.Add_Item(EngMisStr[j]);
#endif
found = true;
break;
}
}
if (!found) {
#ifdef FIXIT_CSII
if (!Session.Scenarios[i]->Get_Official() ||
!((Is_Mission_Counterstrike((char *)(Session.Scenarios[i]->Get_Filename())) && !Is_Counterstrike_Installed()) ||
(Is_Mission_Aftermath((char *)(Session.Scenarios[i]->Get_Filename())) && !Is_Aftermath_Installed())))
#endif
scenariolist.Add_Item(Session.Scenarios[i]->Description());
}
}

if (!scenariolist.Count()) {
Load_Title_Page(true);
return(false);
}

Session.Options.ScenarioIndex = 0;
scenariolist.Set_Selected_Index(Session.Options.ScenarioIndex);

#ifdef FIXIT_RANDOM_GAME
srand(time(NULL));
Seed = rand();
#endif

Load_Title_Page(true);
CCPalette.Set();

while (process) {
Call_Back();

if (AllSurfaces.SurfacesRestored) {
AllSurfaces.SurfacesRestored = FALSE;
display = REDRAW_ALL;
}

if (display) {
if (housebtn.IsDropped) {
housebtn.Collapse();
display = REDRAW_BACKGROUND;
}
Hide_Mouse();

if (display >= REDRAW_BACKGROUND) {
Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);
Fancy_Text_Print(TXT_NONE, 0, 0, scheme, TBLACK, TPF_CENTER | TPF_TEXT);
Fancy_Text_Print(TXT_YOUR_NAME, d_name_x + (d_name_w / 2), d_name_y - d_txt6_h,
scheme, TBLACK, TPF_CENTER | TPF_TEXT);
Fancy_Text_Print(TXT_SIDE_COLON, d_house_x + (d_house_w / 2), d_house_y - d_txt6_h,
scheme, TBLACK, TPF_CENTER | TPF_TEXT);
Fancy_Text_Print(TXT_COLOR_COLON,
d_dialog_x + ((d_dialog_w / 4) * 3), d_color_y - d_txt6_h,
scheme, TBLACK, TPF_CENTER | TPF_TEXT);
Fancy_Text_Print(TXT_EASY, difficulty.X, difficulty.Y - 8 * RESFACTOR,
scheme, TBLACK, TPF_TEXT);
Fancy_Text_Print(TXT_HARD, difficulty.X + difficulty.Width,
difficulty.Y - 8 * RESFACTOR, scheme, TBLACK, TPF_RIGHT | TPF_TEXT);
Fancy_Text_Print(TXT_NORMAL, difficulty.X + difficulty.Width / 2,
difficulty.Y - 8 * RESFACTOR, scheme, TBLACK, TPF_CENTER | TPF_TEXT);
Fancy_Text_Print(TXT_SCENARIOS,
d_scenariolist_x + (d_scenariolist_w / 2), d_scenariolist_y - d_txt6_h,
scheme, TBLACK, TPF_CENTER | TPF_TEXT);
Fancy_Text_Print(TXT_COUNT, d_count_x - 2, d_count_y,
scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
Fancy_Text_Print(TXT_LEVEL, d_level_x - 2, d_level_y,
scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
Fancy_Text_Print(TXT_CREDITS_COLON, d_credits_x - 2, d_credits_y,
scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
Fancy_Text_Print(TXT_AI_PLAYERS_COLON, d_aiplayers_x - 2 * RESFACTOR,
d_aiplayers_y, scheme, TBLACK, TPF_TEXT | TPF_RIGHT);
}

if (display >= REDRAW_COLORS) {
for (i = 0; i < MAX_MPLAYER_COLORS; i++) {
LogicPage->Fill_Rect(cbox_x[i] + 1, d_color_y + 1,
cbox_x[i] + 1 + d_color_w - 2,
d_color_y + 1 + d_color_h - 2,
ColorRemaps[i].Box);
Draw_Box(cbox_x[i], d_color_y, d_color_w, d_color_h,
(i == Session.ColorIdx) ? BOXSTYLE_DOWN : BOXSTYLE_RAISED, false);
}
}

if (display >= REDRAW_PARMS) {
sprintf(staticcountbuff, "%d", Session.Options.UnitCount);
staticcount.Set_Text(staticcountbuff);
staticcount.Draw_Me();

if (BuildLevel <= MPLAYER_BUILD_LEVEL_MAX) {
sprintf(staticlevelbuff, "%d ", BuildLevel);
} else {
sprintf(staticlevelbuff, "**");
}
staticlevel.Set_Text(staticlevelbuff);
staticlevel.Draw_Me();

sprintf(staticcreditsbuff, "%d", Session.Options.Credits);
staticcredits.Set_Text(staticcreditsbuff);
staticcredits.Draw_Me();

sprintf(staticaibuff, "%d", Session.Options.AIPlayers);
staticai.Set_Text(staticaibuff);
staticai.Draw_Me();
}

if (display >= REDRAW_BUTTONS) {
commands->Flag_List_To_Redraw();
commands->Draw_All();
}

Show_Mouse();
display = REDRAW_NONE;
}

bool droplist_is_dropped = housebtn.IsDropped;
input = commands->Input();
if (droplist_is_dropped && !housebtn.IsDropped) {
display = REDRAW_BACKGROUND;
}
if (input & KN_BUTTON) {
if (housebtn.IsDropped) {
housebtn.Collapse();
display = REDRAW_BACKGROUND;
}
}

switch (input) {
case KN_LMOUSE:
if (Keyboard->MouseQX > cbox_x[0] &&
Keyboard->MouseQX < (cbox_x[MAX_MPLAYER_COLORS - 1] + d_color_w) &&
Keyboard->MouseQY > d_color_y &&
Keyboard->MouseQY < (d_color_y + d_color_h)) {
Session.PrefColor = (PlayerColorType)((Keyboard->MouseQX - cbox_x[0]) / d_color_w);
Session.ColorIdx = Session.PrefColor;
name_edt.Set_Color(&ColorRemaps[(Session.ColorIdx == PCOLOR_DIALOG_BLUE) ?
PCOLOR_REALLY_BLUE : Session.ColorIdx]);
name_edt.Flag_To_Redraw();
strcpy(Session.Handle, namebuf);
changed = true;
display = REDRAW_COLORS;
}
break;

case (BUTTON_NAME | KN_BUTTON):
strcpy(Session.Handle, namebuf);
changed = true;
break;

case (BUTTON_HOUSE | KN_BUTTON):
Session.House = HousesType(housebtn.Current_Index() + HOUSE_USSR);
strcpy(Session.Handle, namebuf);
changed = true;
display = REDRAW_BACKGROUND;
break;

case (BUTTON_SCENARIOLIST | KN_BUTTON):
if (scenariolist.Current_Index() != Session.Options.ScenarioIndex) {
Session.Options.ScenarioIndex = scenariolist.Current_Index();
changed = true;
}
break;

case (BUTTON_COUNT | KN_BUTTON):
Session.Options.UnitCount = countgauge.Get_Value() + SessionClass::CountMin[Session.Options.Bases];
changed = true;
display = REDRAW_PARMS;
break;

case (BUTTON_LEVEL | KN_BUTTON):
BuildLevel = levelgauge.Get_Value() + 1;
if (BuildLevel > MPLAYER_BUILD_LEVEL_MAX) {
BuildLevel = MPLAYER_BUILD_LEVEL_MAX;
}
changed = true;
display = REDRAW_PARMS;
break;

case (BUTTON_CREDITS | KN_BUTTON):
Session.Options.Credits = creditsgauge.Get_Value();
Session.Options.Credits = ((Session.Options.Credits + 250) / 500) * 500;
changed = true;
display = REDRAW_PARMS;
break;

case (BUTTON_AIPLAYERS | KN_BUTTON):
Session.Options.AIPlayers = aiplayersgauge.Get_Value() + 1;
if (Session.Options.AIPlayers + 1 >= Rule.MaxPlayers) {
Session.Options.AIPlayers = Rule.MaxPlayers - 1;
aiplayersgauge.Set_Value(Session.Options.AIPlayers - 1);
}
changed = true;
display = REDRAW_PARMS;
break;

case (BUTTON_OPTIONS | KN_BUTTON):
if (Session.Options.Bases != optionlist.Is_Checked(0)) {
Session.Options.Bases = optionlist.Is_Checked(0);
if (Session.Options.Bases) {
Session.Options.UnitCount = Fixed_To_Cardinal(
SessionClass::CountMax[1] - SessionClass::CountMin[1],
Cardinal_To_Fixed(
SessionClass::CountMax[0] - SessionClass::CountMin[0],
Session.Options.UnitCount - SessionClass::CountMin[0])) +
SessionClass::CountMin[1];
} else {
Session.Options.UnitCount = Fixed_To_Cardinal(
SessionClass::CountMax[0] - SessionClass::CountMin[0],
Cardinal_To_Fixed(
SessionClass::CountMax[1] - SessionClass::CountMin[1],
Session.Options.UnitCount - SessionClass::CountMin[1])) +
SessionClass::CountMin[0];
}
countgauge.Set_Maximum(
SessionClass::CountMax[Session.Options.Bases] - SessionClass::CountMin[Session.Options.Bases]);
countgauge.Set_Value(Session.Options.UnitCount - SessionClass::CountMin[Session.Options.Bases]);
}
Session.Options.Tiberium = optionlist.Is_Checked(1);
Special.IsTGrowth = Session.Options.Tiberium;
Rule.IsTGrowth = Session.Options.Tiberium;
Special.IsTSpread = Session.Options.Tiberium;
Rule.IsTSpread = Session.Options.Tiberium;
Session.Options.Goodies = optionlist.Is_Checked(2);
Special.IsShadowGrow = optionlist.Is_Checked(3);
changed = true;
display = REDRAW_PARMS;
break;

case (BUTTON_OK | KN_BUTTON):
rc = true;
process = false;
break;

case KN_ESC:
case (BUTTON_CANCEL | KN_BUTTON):
process = false;
rc = false;
break;

default:
break;
}
}

if (rc) {
NodeNameType *who;
Session.NumPlayers = 1;
Scen.Scenario = Session.Options.ScenarioIndex;
strcpy(Scen.ScenarioName, Session.Scenarios[Session.Options.ScenarioIndex]->Get_Filename());

who = new NodeNameType;
strcpy(who->Name, namebuf);
who->Player.House = Session.House;
who->Player.Color = Session.ColorIdx;
who->Player.ProcessTime = -1;
Session.Players.Add(who);

int diff = difficulty.Get_Value() * (Rule.IsFineDifficulty ? 1 : 2);
switch (diff) {
case 0:
Scen.CDifficulty = DIFF_HARD;
Scen.Difficulty = DIFF_EASY;
break;
case 1:
Scen.CDifficulty = DIFF_HARD;
Scen.Difficulty = DIFF_NORMAL;
break;
case 2:
Scen.CDifficulty = DIFF_NORMAL;
Scen.Difficulty = DIFF_NORMAL;
break;
case 3:
Scen.CDifficulty = DIFF_EASY;
Scen.Difficulty = DIFF_NORMAL;
break;
default:
Scen.CDifficulty = DIFF_EASY;
Scen.Difficulty = DIFF_HARD;
break;
}
}

while (scenariolist.Count()) {
item = (char *)scenariolist.Get_Item(0);
scenariolist.Remove_Item(item);
}

Hide_Mouse();
Load_Title_Page(true);
Show_Mouse();

if (changed) {
Session.Write_MultiPlayer_Settings();
}

return(rc);
}

/***********************************************************************************************
 * Find_Local_Scenario -- finds the file name of the scenario with matching attributes         *
 ***********************************************************************************************/
bool Find_Local_Scenario(char *description, char *filename, uint32_t length, char *digest, bool official)
{
char digest_buffer[32];

for (int index = 0; index < Session.Scenarios.Count(); index++) {
if (!strcmp(Session.Scenarios[index]->Description(), description)) {
CCFileClass file(Session.Scenarios[index]->Get_Filename());
if (file.Is_Available() && file.Size() == length) {
if (!official) {
INIClass ini;
ini.Load(file);
ini.Get_String("Digest", "1", "No digest here mate. Nope.", digest_buffer, sizeof(digest_buffer));
}
#ifdef FIXIT_CSII
if (Is_Mission_Aftermath((char *)Session.Scenarios[index]->Get_Filename())) {
strcpy(filename, Session.Scenarios[index]->Get_Filename());
return(true);
}
#endif
if (official || !strcmp(digest, digest_buffer)) {
strcpy(filename, Session.Scenarios[index]->Get_Filename());
return(true);
}
}
}
}

return(false);
}
