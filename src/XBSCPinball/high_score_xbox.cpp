#include "pch.h"
#include "high_score.h"
#include "options.h"
#include <string.h>

bool high_score::dlg_enter_name = false;
bool high_score::ShowDialog = false;
high_score_entry high_score::DlgData{};
std::vector<high_score_entry> high_score::ScoreQueue{};
high_score_struct high_score::highscore_table[5]{};

void high_score::clear_table()
{
    for (int i=0;i<5;++i) { highscore_table[i].Score=-999; highscore_table[i].Name[0]=0; }
}
int high_score::read()
{
    clear_table();
    for (int i=0;i<5;++i)
    {
        char k[32]; snprintf(k,sizeof(k),"%d.Name",i); std::string n=options::GetSetting(k,"");
        strncpy(highscore_table[i].Name,n.c_str(),31); highscore_table[i].Name[31]=0;
        snprintf(k,sizeof(k),"%d.Score",i); highscore_table[i].Score=options::get_int(k,highscore_table[i].Score);
    }
    return 0;
}
int high_score::write()
{
    for (int i=0;i<5;++i)
    {
        char k[32]; snprintf(k,sizeof(k),"%d.Name",i); options::SetSetting(k,highscore_table[i].Name);
        snprintf(k,sizeof(k),"%d.Score",i); options::set_int(k,highscore_table[i].Score);
    }
    return 0;
}
int high_score::get_score_position(int score)
{
    if (score<=0) return -1; for(int i=0;i<5;++i) if(highscore_table[i].Score<score) return i; return -1;
}
void high_score::place_new_score_into(high_score_entry d)
{
    if(d.Position<0||d.Position>=5) return; for(int i=4;i>d.Position;--i) highscore_table[i]=highscore_table[i-1];
    d.Entry.Name[31]=0; highscore_table[d.Position]=d.Entry;
}
void high_score::show_high_score_dialog() {}
void high_score::show_and_set_high_score_dialog(high_score_entry score)
{
    if(score.Position<0||score.Position>=5) score.Position=get_score_position(score.Entry.Score);
    if(score.Position>=0) { if(!score.Entry.Name[0]) strcpy(score.Entry.Name,"PLAYER"); place_new_score_into(score); }
}
void high_score::RenderHighScoreDialog() {}
