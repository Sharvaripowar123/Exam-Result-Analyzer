/*
 ============================================================
  EXAM RESULT ANALYZER v3.0
  B.Tech NEP-2.0 | Data Science | Shivaji University
 ============================================================
  Compile:
    g++ -o exam.exe exam_v3.cpp -I"C:\curl\include" -L"C:\curl\lib" -lcurl -lssl -lcrypto -lws2_32
 ============================================================
*/
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <curl/curl.h>
#include <windows.h>
using namespace std;
// ============================================================
//  CONFIG
// ============================================================
const string API_URL   = "https://sukapps.unishivaji.ac.in/exam_pro_api/api/MarksheetController/RPT_ONLINEMARKSSTATEMENT";
const string PDF_DIR   = "pdfs";
const string SAVE_FILE = "result.csv";
const int MAX_S   = 200;
const int MAX_SUB = 15;

const vector<string> POPPLER_PATHS = {
    "pdftotext",
    "C:\\ExamAnalyzer\\poppler-25.12.0\\Library\\bin\\pdftotext.exe",
    ".\\poppler-25.12.0\\Library\\bin\\pdftotext.exe",
    ".\\poppler\\Library\\bin\\pdftotext.exe",
    "C:\\poppler\\Library\\bin\\pdftotext.exe",
};
// ============================================================
//  STRUCTS
// ============================================================
struct SubjectMarks {
    float mse=0, ise=0, ese=0, poe=0, total=0;
    int   maxTotal=100;
    bool  passed=false;
};

struct AtktEntry {
    string semLabel, subjectName;
};

struct Student {
    int   srNo=0, rank=0, numSubjects=0;
    string name, rollNo, prnNo, seatNo;
    string mdmSubject, oeSubject, oeLabSubject;
    SubjectMarks marks[MAX_SUB];
    float grandTotal=0, percentage=0;
    bool  overallPass=false;
    char  grade='F';
    string sem3Atkt;             // Sem 3 failed subjects
    vector<AtktEntry> prevAtkt;  // Previous sem backlogs
};

// ============================================================
//  GLOBALS
// ============================================================
Student students[MAX_S];
string  subjectNames[MAX_SUB], subjectSlot[MAX_SUB];
int     subjectType[MAX_SUB];
int     numStudents=0, numSubjects=0;
string  g_popplerPath;

// ============================================================
//  UTILITIES
// ============================================================
string trim(const string &s) {
    int a=s.find_first_not_of(" \t\r\n"), b=s.find_last_not_of(" \t\r\n");
    return (a==(int)string::npos)?"":s.substr(a,b-a+1);
}
string trunc(const string &s, int w) {
    return ((int)s.size()<=w)?s:s.substr(0,w-2)+"..";
}
char calcGrade(float p) {
    if(p>=75) return 'A'; if(p>=60) return 'B';
    if(p>=50) return 'C'; if(p>=40) return 'D'; return 'F';
}
string slotLabel(int i) {
    return subjectSlot[i].empty()?trunc(subjectNames[i],8):subjectSlot[i];
}
string typeStr(int t) {
    if(t==0) return "Theory"; if(t==1) return "Practical"; return "ISE-Only";
}
void wait() { cout<<"\n  Press Enter to continue..."; cin.ignore(10000,'\n'); cin.get(); }
void drawLine(char c='-', int n=60) { for(int i=0;i<n;i++) cout<<c; cout<<"\n"; }
void cls() { system("cls"); }
void mkDir(const string &d) { system(("mkdir \""+d+"\" 2>nul").c_str()); }
void header() {
    cout<<"\n"; drawLine('=',65);
    cout<<setw(45)<<"EXAM RESULT ANALYZER\n";
    cout<<setw(50)<<"B.Tech NEP-2.0 | Data Science\n";
    cout<<setw(50)<<"Shivaji University, Kolhapur\n";
    drawLine('=',65); cout<<"\n";
}

// ============================================================
//  POPPLER
// ============================================================
bool testPoppler(const string &p) {
    return system(("\""+p+"\" -v >nul 2>&1").c_str())==0;
}
void setupPoppler() {
    ifstream f(".poppler_path");
    if(f.is_open()){string p;getline(f,p);f.close();p=trim(p);if(!p.empty()&&testPoppler(p)){g_popplerPath=p;return;}}
    for(const string &p:POPPLER_PATHS) if(testPoppler(p)){g_popplerPath=p;ofstream sf(".poppler_path");if(sf.is_open()){sf<<p;sf.close();}cout<<"  [OK] pdftotext: "<<p<<"\n";return;}
    cout<<"  Enter pdftotext.exe path: "; string p; getline(cin,p); p=trim(p);
    if(!p.empty()&&p.front()=='"') p=p.substr(1);
    if(!p.empty()&&p.back()=='"')  p.pop_back();
    g_popplerPath=p; ofstream sf(".poppler_path"); if(sf.is_open()){sf<<p;sf.close();}
}

// ============================================================
//  CURL
// ============================================================
size_t curlWrite(void *ptr, size_t size, size_t nmemb, vector<char> *buf) {
    size_t t=size*nmemb; buf->insert(buf->end(),(char*)ptr,(char*)ptr+t); return t;
}
bool downloadPDF(const string &prn, const string &pdfPath) {
    CURL *curl=curl_easy_init(); if(!curl) return false;
    vector<char> buf; string body="{\"PRN\":\""+prn+"\"}";
    struct curl_slist *hdrs=nullptr;
    hdrs=curl_slist_append(hdrs,"Content-Type: application/json");
    hdrs=curl_slist_append(hdrs,"Accept: application/json, text/plain, */*");
    hdrs=curl_slist_append(hdrs,"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    curl_easy_setopt(curl,CURLOPT_URL,API_URL.c_str());
    curl_easy_setopt(curl,CURLOPT_POST,1L);
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,body.c_str());
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,hdrs);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,curlWrite);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&buf);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,0L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,0L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,30L);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
    CURLcode res=curl_easy_perform(curl);
    long httpCode=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&httpCode);
    curl_slist_free_all(hdrs); curl_easy_cleanup(curl);
    if(res!=CURLE_OK||(int)buf.size()<1000) return false;
    FILE *fp=fopen(pdfPath.c_str(),"wb"); if(!fp) return false;
    fwrite(buf.data(),1,buf.size(),fp); fclose(fp); return true;
}

// ============================================================
//  PDF -> TEXT
// ============================================================
bool pdfToText(const string &pdf, const string &txt) {
    if(g_popplerPath.empty()) return false;
    string cmd="\"\""+g_popplerPath+"\" -layout \""+pdf+"\" \""+txt+"\"\" >nul 2>&1";
    system(cmd.c_str());
    ifstream chk(txt); bool ok=chk.is_open()&&chk.peek()!=EOF; chk.close(); return ok;
}

// ============================================================
//  PARSE MARKS — handle $, Ab, special chars
// ============================================================
float parseMarks(const string &s) {
    string clean="";
    for(char c:s) if(isdigit(c)||c=='.') clean+=c;
    if(clean.empty()) return 0;
    try{return stof(clean);}catch(...){return 0;}
}

bool isCatToken(const string &s) {
    return s.find("ESEx")!=string::npos||s.find("MSE-")!=string::npos||
           s.find("ISE(")!=string::npos||s.find("POE(")!=string::npos||
           s.find("MSE(")!=string::npos;
}
string catType(const string &t) {
    if(t.find("ESE")!=string::npos) return "ESE";
    if(t.find("MSE")!=string::npos) return "MSE";
    if(t.find("POE")!=string::npos) return "POE";
    if(t.find("ISE")!=string::npos) return "ISE";
    return "";
}

// ============================================================
//  MAIN PARSE FUNCTION
//  Logic:
//  1. Find "Bachelor of Technology NEP-2.0 Part 2 Sem 3"
//  2. Parse all subjects under it
//  3. Read "Sem - 3 Result - PASS/FAIL" → overallPass
//  4. Everything ABOVE Sem3 section → previous ATKT
// ============================================================
bool parseStudent(const string &txtPath, Student &s) {
    ifstream f(txtPath);
    if(!f.is_open()) return false;
    vector<string> lines; string ln;
    while(getline(f,ln)) lines.push_back(ln);
    f.close();
    if(lines.empty()) return false;

    // --- Name, Seat, PRN ---
    for(const string &l:lines) {
        if(l.find("Name")!=string::npos&&l.find("Seat No")!=string::npos&&l.find("Mother")==string::npos) {
            size_t nc=l.find(':');
            if(nc!=string::npos){
                string rest=l.substr(nc+1);
                size_t sp=rest.find("Seat No");
                if(sp!=string::npos){
                    s.name=trim(rest.substr(0,sp));
                    size_t sc=rest.find(':',sp);
                    if(sc!=string::npos){
                        s.seatNo=trim(rest.substr(sc+1));
                        // Remove any extra chars after seat number
                        string &sn=s.seatNo;
                        int end=0;
                        while(end<(int)sn.size()&&(isdigit(sn[end])||isalpha(sn[end]))) end++;
                        sn=sn.substr(0,end);
                    }
                }
            }
            break;
        }
    }
    for(const string &l:lines)
        if(l.find("University PRN")!=string::npos){size_t c=l.find(':');if(c!=string::npos)s.prnNo=trim(l.substr(c+1));break;}

    // --- Find Sem 3 section start ---
    // Strategy: Find the LAST "Paper Code" header that comes AFTER a "Part 2 Sem 3" line
    // Layout: ... "Part 2 Sem 3" line ... "Paper Code" header ... subjects ...
    
    // Step 1: Find all "Part 2 Sem 3" line positions
    int lastSem3Line = -1;
    for(int i=0;i<(int)lines.size();i++){
        if(trim(lines[i]).find("Part 2 Sem 3")!=string::npos)
            lastSem3Line = i;
    }
    if(lastSem3Line < 0) return false;

    // Step 2: sem3Start = FIRST "Part 2 Sem 3" line
    // We parse ALL sections including page 2
    int sem3Start = -1;
    for(int i=0;i<(int)lines.size();i++){
        if(trim(lines[i]).find("Part 2 Sem 3")!=string::npos){
            sem3Start = i;
            break; // first occurrence
        }
    }
    if(sem3Start < 0) return false;

    // --- Parse PREVIOUS SEM ATKT (everything before sem3Start) ---
    string curSemLabel="";
    for(int i=0;i<sem3Start;i++) {
        string tl=trim(lines[i]);
        if(tl.empty()) continue;
        // Detect sem section header e.g. "Bachelor of Technology NEP-2.0 Part 1 Sem 1"
        if(tl.find("Bachelor of Technology")!=string::npos) {
            // Extract "Sem X" label
            size_t sp=tl.find("Sem ");
            if(sp!=string::npos) curSemLabel=trim(tl.substr(sp,5));
            else curSemLabel="";
            continue;
        }
        // Line with "Sem - X Result - FAIL" → don't add, skip
        if(tl.find("Sem -")!=string::npos||tl.find("Part -")!=string::npos) continue;

        // 6-digit paper code line with FAIL in subject result column
        if((int)tl.size()>=6&&!curSemLabel.empty()) {
            string c6=tl.substr(0,6); bool allD=true;
            for(char c:c6) if(!isdigit(c)){allD=false;break;}
            if(allD) {
                // Subject name is between code and first cat keyword
                string rest=trim(tl.substr(6));
                size_t catPos=string::npos;
                for(const string &kw:{"ESEx","MSE-","ISE(","POE(","MSE("}) {
                    size_t p=rest.find(kw);
                    if(p!=string::npos&&(catPos==string::npos||p<catPos)) catPos=p;
                }
                string subjName=(catPos!=string::npos)?trim(rest.substr(0,catPos)):rest;
                // Check if subject result is FAIL (last column of this line)
                // Subject result is after: CAT marks PASS/FAIL subjtotal PASS/FAIL
                // Just check if line contains FAIL after the subject name
                string afterName=(catPos!=string::npos)?rest.substr(catPos):"";
                // Count PASS/FAIL occurrences — last one is subject result
                bool subjFail=false;
                size_t pos=0;
                string lastResult="";
                while(pos<afterName.size()){
                    size_t pf=afterName.find("PASS",pos);
                    size_t ff=afterName.find("FAIL",pos);
                    if(pf==string::npos&&ff==string::npos) break;
                    if(ff==string::npos||(pf!=string::npos&&pf<ff)){lastResult="PASS";pos=pf+4;}
                    else{lastResult="FAIL";pos=ff+4;}
                }
                if(lastResult=="FAIL") subjFail=true;
                if(subjFail&&!subjName.empty()){
                    AtktEntry e; e.semLabel=curSemLabel; e.subjectName=subjName;
                    s.prevAtkt.push_back(e);
                }
            }
        }
    }

    // --- Parse SEM 3 SUBJECTS ---
    struct TempSubj {
        string code,name;
        float ese=0,mse=0,ise=0,poe=0,total=0;
        bool passed=false,hasESE=false,hasPOE=false;
    };
    vector<TempSubj> ts;
    TempSubj cur; bool inTable=true,hasCur=false;
    bool sem3Pass=false;
    bool inSem3Block=true; // start in sem3 block

    for(int i=sem3Start;i<(int)lines.size();i++) {
        string tl=trim(lines[i]);
        if(tl.empty()) continue;

        // Track which section we are in
        if(tl.find("Part 2 Sem 3")!=string::npos){inSem3Block=true;continue;}
        if(tl.find("Part 1 Sem")!=string::npos||tl.find("Part 2 Sem")!=string::npos){
            if(tl.find("Sem 3")==string::npos){inSem3Block=false;} continue;
        }
        if(!inSem3Block) continue;

        // Table header (optional - skip if present)
        if(tl.find("Paper Code")!=string::npos){inTable=true;continue;}

        // Sem 3 result line → set overallPass
        // Don't break - continue to parse page 2 subjects
        if(tl.find("Sem -")!=string::npos) {
            if(tl.find("Sem - 3")!=string::npos||tl.find("Sem-3")!=string::npos){
                if(hasCur){ts.push_back(cur);hasCur=false;}
                sem3Pass=(tl.find("PASS")!=string::npos);
                // Don't break - page 2 may have more subjects
            }
            continue; // skip Sem-1, Sem-2 result lines
        }
        // Stop at Part-2 Result line (after Sem-3 result)
        if(tl.find("Part - 2")!=string::npos||tl.find("Part-2")!=string::npos){
            if(hasCur){ts.push_back(cur);hasCur=false;}
            break;
        }
        // Skip page break lines but don't stop
        if(tl.find("Page 1")!=string::npos||tl.find("Page 2")!=string::npos) continue;
        // Skip repeated Bachelor Sem3 header on page 2
        if(tl.find("Part 2 Sem 3")!=string::npos) continue;

        if(!inTable) continue;
        if(tl.find("Paper / Subject")!=string::npos) continue;
        if(tl.find("Category")!=string::npos&&tl.find("Marks")!=string::npos) continue;
        if(tl.find("Part -")!=string::npos||tl.find("Result Declared")!=string::npos) continue;
        if(tl.find("Examination :")!=string::npos||tl.find("SHIVAJI UNIVERSITY")!=string::npos) continue;
        if(tl.find("Online Statement")!=string::npos) continue;

        // 6-digit paper code?
        bool hasCode=false; string code6;
        if((int)tl.size()>=6){
            code6=tl.substr(0,6); bool allD=true;
            for(char c:code6) if(!isdigit(c)){allD=false;break;}
            if(allD) hasCode=true;
        }

        if(hasCode) {
            if(hasCur) ts.push_back(cur);
            cur=TempSubj(); cur.code=code6; hasCur=true;
            string rest=trim(tl.substr(6));
            size_t catPos=string::npos;
            for(const string &kw:{"ESEx","MSE-","ISE(","POE(","MSE("}) {
                size_t p=rest.find(kw);
                if(p!=string::npos&&(catPos==string::npos||p<catPos)) catPos=p;
            }
            if(catPos!=string::npos) {
                cur.name=trim(rest.substr(0,catPos));
                string catPart=rest.substr(catPos);
                istringstream ss(catPart);
                string catTok,marksStr,res1,res2; float tot;
                if(ss>>catTok>>marksStr>>res1) {
                    float marks=parseMarks(marksStr);
                    string ct=catType(catTok);
                    if(ct=="ESE"){cur.ese=marks;cur.hasESE=true;}
                    else if(ct=="MSE") cur.mse=marks;
                    else if(ct=="ISE") cur.ise=marks;
                    else if(ct=="POE"){cur.poe=marks;cur.hasPOE=true;}
                    // Subject total and result (last two tokens)
                    string totStr;
                    if(ss>>totStr>>res2){cur.total=parseMarks(totStr);cur.passed=(res2=="PASS");}
                }
            } else cur.name=rest;

        } else if(hasCur&&isCatToken(tl)) {
            istringstream ss(tl); string catTok,marksStr,res;
            if(ss>>catTok>>marksStr>>res) {
                float marks=parseMarks(marksStr);
                string ct=catType(catTok);
                if(ct=="ESE"){cur.ese=marks;cur.hasESE=true;}
                else if(ct=="MSE") cur.mse=marks;
                else if(ct=="ISE") cur.ise=marks;
                else if(ct=="POE"){cur.poe=marks;cur.hasPOE=true;}
            }
        }
    }
    if(hasCur) ts.push_back(cur);
    if(ts.empty()) return false;

    // --- Populate subject metadata (first student only) ---
    if(numSubjects==0) {
        numSubjects=min((int)ts.size(),MAX_SUB);
        for(int i=0;i<numSubjects;i++) {
            subjectNames[i]=ts[i].name;
            subjectType[i]=ts[i].hasESE?0:ts[i].hasPOE?1:2;
            string nm=subjectNames[i];
            transform(nm.begin(),nm.end(),nm.begin(),::tolower);
            // MDM = Building Construction OR Electrical
            if(nm.find("building")!=string::npos||
               nm.find("electrical")!=string::npos||
               nm.find("introduction to electrical")!=string::npos)
                subjectSlot[i]="MDM";
            // OE = Entrepreneurship OR Environment & Development (non-lab)
            else if((nm.find("entrepreneurship")!=string::npos&&nm.find("lab")==string::npos)||
                    (nm.find("environment")!=string::npos&&nm.find("lab")==string::npos))
                subjectSlot[i]="OE";
            // OE Lab = Entrepreneurship Lab OR Environment Lab
            else if(nm.find("entrepreneurship lab")!=string::npos||
                    (nm.find("environment")!=string::npos&&nm.find("lab")!=string::npos))
                subjectSlot[i]="OE Lab";
            else subjectSlot[i]="";
        }
    }

    // --- Fill marks ---
    s.numSubjects=numSubjects;
    s.grandTotal=0; s.sem3Atkt="";

    for(int i=0;i<numSubjects&&i<(int)ts.size();i++) {
        SubjectMarks &m=s.marks[i];
        m.ese=ts[i].ese; m.mse=ts[i].mse;
        m.ise=ts[i].ise; m.poe=ts[i].poe;
        m.total=ts[i].total; m.passed=ts[i].passed;
        m.maxTotal=(subjectType[i]==0)?100:(subjectType[i]==1)?75:50;
        // Sem 3 ATKT = only truly failed subjects
        if(!m.passed) s.sem3Atkt+=subjectNames[i]+" ";
        s.grandTotal+=m.total;
    }

    // Official Sem 3 result from "Sem - 3 Result - PASS/FAIL" line
    s.overallPass=sem3Pass;
    s.percentage=(s.grandTotal/800.0f)*100.0f;
    s.grade=calcGrade(s.percentage);

    // MDM/OE per-student detection
    s.mdmSubject=s.oeSubject=s.oeLabSubject="";
    for(int i=0;i<(int)ts.size()&&i<numSubjects;i++) {
        string nm=ts[i].name;
        string nml=nm;
        transform(nml.begin(),nml.end(),nml.begin(),::tolower);

        // MDM = Building Construction (Civil) OR Electrical
        if(nml.find("building")!=string::npos)
            s.mdmSubject="Civil";
        else if(nml.find("electrical")!=string::npos||nml.find("introduction to electrical")!=string::npos)
            s.mdmSubject="Electrical";

        // OE = Entrepreneurship OR Environment & Development (non-lab)
        if(nml.find("entrepreneurship")!=string::npos&&nml.find("lab")==string::npos)
            s.oeSubject="Entrepreneurship";
        else if(nml.find("environment")!=string::npos&&nml.find("lab")==string::npos)
            s.oeSubject="Env & Dev";

        // OE Lab
        if(nml.find("entrepreneurship lab")!=string::npos)
            s.oeLabSubject="Entrep.Lab";
        else if(nml.find("environment")!=string::npos&&nml.find("lab")!=string::npos)
            s.oeLabSubject="Env.Lab";
    }
    s.rollNo=to_string(numStudents+1);
    return true;
}

// ============================================================
//  RANKS
// ============================================================
void assignRanks() {
    int idx[MAX_S]; for(int i=0;i<numStudents;i++) idx[i]=i;
    for(int i=0;i<numStudents-1;i++)
        for(int j=0;j<numStudents-1-i;j++)
            if(students[idx[j]].percentage<students[idx[j+1]].percentage)
            {int t=idx[j];idx[j]=idx[j+1];idx[j+1]=t;}
    for(int i=0;i<numStudents;i++) students[idx[i]].rank=i+1;
}

// ============================================================
//  SAVE / LOAD
// ============================================================
void saveToFile() {
    ofstream f(SAVE_FILE); if(!f.is_open()) return;
    f<<numSubjects;
    for(int i=0;i<numSubjects;i++) f<<","<<subjectNames[i]<<"|"<<subjectType[i]<<"|"<<subjectSlot[i];
    f<<"\n";
    for(int i=0;i<numStudents;i++) {
        Student &s=students[i];
        string s3=s.sem3Atkt.empty()?"-":s.sem3Atkt;
        for(char &c:s3) if(c==',') c=';';
        f<<s.srNo<<","<<s.rollNo<<","<<s.prnNo<<","<<s.seatNo<<","<<s.name<<","
         <<s3<<","<<s.mdmSubject<<","<<s.oeSubject<<","<<s.oeLabSubject<<","
         <<(s.overallPass?1:0);
        for(int j=0;j<numSubjects;j++)
            f<<","<<s.marks[j].mse<<","<<s.marks[j].ise<<","<<s.marks[j].ese<<","<<s.marks[j].poe<<","<<s.marks[j].total<<","<<s.marks[j].passed;
        f<<"|";
        for(auto &e:s.prevAtkt) f<<e.semLabel<<"~"<<e.subjectName<<"^";
        f<<"\n";
    }
    f.close();
}

void loadFromFile() {
    ifstream f(SAVE_FILE); if(!f.is_open()) return;
    string ln; if(!getline(f,ln)){f.close();return;}
    int cp=ln.find(','); numSubjects=stoi(ln.substr(0,cp)); ln=ln.substr(cp+1);
    for(int i=0;i<numSubjects;i++) {
        int pos=ln.find(',');
        string tok=(pos==(int)string::npos)?ln:ln.substr(0,pos);
        if(pos!=(int)string::npos) ln=ln.substr(pos+1);
        int p1=tok.find('|');
        if(p1!=(int)string::npos){
            subjectNames[i]=tok.substr(0,p1); string rest=tok.substr(p1+1);
            int p2=rest.find('|');
            if(p2!=(int)string::npos){subjectType[i]=stoi(rest.substr(0,p2));subjectSlot[i]=rest.substr(p2+1);}
            else{subjectType[i]=stoi(rest);subjectSlot[i]="";}
        } else{subjectNames[i]=tok;subjectType[i]=0;subjectSlot[i]="";}
    }
    numStudents=0;
    while(getline(f,ln)&&numStudents<MAX_S) {
        Student &s=students[numStudents];
        auto nxt=[&](string &str)->string{
            int p=str.find(','); string t;
            if(p==(int)string::npos){t=str;str="";}
            else{t=str.substr(0,p);str=str.substr(p+1);}
            return t;
        };
        s.srNo=stoi(nxt(ln)); s.rollNo=nxt(ln); s.prnNo=nxt(ln);
        s.seatNo=nxt(ln); s.name=nxt(ln);
        string s3=nxt(ln); s.sem3Atkt=(s3=="-")?"":s3;
        for(char &c:s.sem3Atkt) if(c==';') c=' ';
        s.mdmSubject=nxt(ln); s.oeSubject=nxt(ln);
        string olField=nxt(ln); s.oeLabSubject=olField;
        s.overallPass=(stoi(nxt(ln))==1);
        s.numSubjects=numSubjects;
        s.grandTotal=0;
        for(int j=0;j<numSubjects;j++){
            s.marks[j].mse=stof(nxt(ln)); s.marks[j].ise=stof(nxt(ln));
            s.marks[j].ese=stof(nxt(ln)); s.marks[j].poe=stof(nxt(ln));
            // total and passed stored now
            string totStr=nxt(ln); string pasStr=nxt(ln);
            // Handle pipe in last field
            int pipe=pasStr.find('|');
            string atktPart="";
            if(pipe!=(int)string::npos){atktPart=pasStr.substr(pipe+1);pasStr=pasStr.substr(0,pipe);}
            // Also check ln for pipe
            int lnPipe=ln.find('|');
            if(lnPipe!=(int)string::npos&&atktPart.empty()){atktPart=ln.substr(lnPipe+1);ln=ln.substr(0,lnPipe);}
            s.marks[j].total=stof(totStr);
            s.marks[j].passed=(pasStr=="1");
            s.marks[j].maxTotal=(subjectType[j]==0)?100:(subjectType[j]==1)?75:50;
            s.grandTotal+=s.marks[j].total;
            // Parse prevAtkt from atktPart
            if(!atktPart.empty()){
                string tmp=atktPart;
                while(!tmp.empty()){
                    int cp2=tmp.find('^');
                    string entry=(cp2!=(int)string::npos)?tmp.substr(0,cp2):tmp;
                    if(cp2!=(int)string::npos) tmp=tmp.substr(cp2+1); else tmp="";
                    int tp=entry.find('~');
                    if(tp!=(int)string::npos){
                        AtktEntry e; e.semLabel=entry.substr(0,tp); e.subjectName=entry.substr(tp+1);
                        if(!e.subjectName.empty()) s.prevAtkt.push_back(e);
                    }
                }
            }
        }
        s.percentage=(s.grandTotal/800.0f)*100.0f;
        s.grade=calcGrade(s.percentage);
        numStudents++;
    }
    f.close();
    if(numStudents>0){assignRanks();cout<<"  Loaded "<<numStudents<<" records.\n";}
}

// ============================================================
//  FETCH
// ============================================================
bool fetchStudent(const string &prn, Student &s) {
    mkDir(PDF_DIR);
    string pdf=PDF_DIR+"\\"+prn+".pdf", txt=PDF_DIR+"\\"+prn+".txt";
    ifstream pc(pdf,ios::binary|ios::ate);
    bool hasPdf=(pc.is_open()&&pc.tellg()>1000); pc.close();
    if(!hasPdf){cout<<"  Downloading: "<<prn<<"...\n";if(!downloadPDF(prn,pdf)) return false;}
    else cout<<"  [Cache] PDF: "<<prn<<"\n";
    ifstream tc(txt); bool hasTxt=(tc.is_open()&&tc.peek()!=EOF); tc.close();
    if(!hasTxt){cout<<"  Converting...\n";if(!pdfToText(pdf,txt)){cout<<"  !! Convert failed\n";return false;}}
    else cout<<"  [Cache] TXT: "<<prn<<"\n";
    cout<<"  Parsing...\n";
    if(!parseStudent(txt,s)){cout<<"  !! Parse failed\n";return false;}
    cout<<"  [OK] "<<s.name<<" | Seat: "<<s.seatNo<<" | Total: "<<(int)s.grandTotal<<" | "<<(s.overallPass?"PASS":"FAIL")<<"\n";
    return true;
}

// ============================================================
//  TABLE
// ============================================================
int tableWidth(){
    int W=4+5+12+6+22;
    for(int i=0;i<numSubjects;i++) W+=(subjectType[i]==0)?20:(subjectType[i]==1)?15:10;
    return W+7+8+5+6+4;
}
void printHeader(){
    int W=tableWidth(); drawLine('=',W);
    cout<<left<<setw(4)<<"Sr"<<setw(5)<<"Roll"<<setw(12)<<"PRN"<<setw(6)<<"Seat"<<setw(22)<<"Name";
    for(int i=0;i<numSubjects;i++){
        int cw=(subjectType[i]==0)?20:(subjectType[i]==1)?15:10;
        string sl=slotLabel(i); int pad=cw-(int)sl.size(),pl=pad/2;
        cout<<string(pl,' ')<<sl<<string(pad-pl,' ');
    }
    cout<<right<<setw(7)<<"Total"<<setw(8)<<"Pct(%)"<<setw(5)<<"Grd"<<setw(6)<<"Sem3"<<setw(4)<<"Rnk"<<"\n";
    cout<<left<<setw(4)<<""<<setw(5)<<""<<setw(12)<<""<<setw(6)<<""<<setw(22)<<"";
    for(int i=0;i<numSubjects;i++){
        if(subjectType[i]==0) cout<<right<<setw(5)<<"MSE"<<setw(5)<<"ISE"<<setw(5)<<"ESE"<<setw(5)<<"Tot";
        else if(subjectType[i]==1) cout<<right<<setw(5)<<"POE"<<setw(5)<<"ISE"<<setw(5)<<"Tot";
        else cout<<right<<setw(5)<<"ISE"<<setw(5)<<"Tot";
    }
    cout<<right<<setw(7)<<"---"<<setw(8)<<"------"<<setw(5)<<"---"<<setw(6)<<"----"<<setw(4)<<"---"<<"\n";
    drawLine('=',W);
}
void printRow(const Student &s){
    cout<<left<<setw(4)<<s.srNo<<setw(5)<<s.rollNo<<setw(12)<<trunc(s.prnNo,11)<<setw(6)<<s.seatNo<<setw(22)<<trunc(s.name,21);
    for(int i=0;i<s.numSubjects;i++){
        const SubjectMarks &m=s.marks[i];
        if(subjectType[i]==0){
            string ms=to_string((int)m.mse)+(m.mse<12?"*":"");
            string es=to_string((int)m.ese)+(m.ese<24?"*":"");
            string ts=to_string((int)m.total)+(!m.passed?"!":"");
            cout<<right<<setw(5)<<ms<<setw(5)<<(int)m.ise<<setw(5)<<es<<setw(5)<<ts;
        } else if(subjectType[i]==1){
            string ts=to_string((int)m.total)+(!m.passed?"!":"");
            cout<<right<<setw(5)<<(int)m.poe<<setw(5)<<(int)m.ise<<setw(5)<<ts;
        } else {
            string ts=to_string((int)m.total)+(!m.passed?"!":"");
            cout<<right<<setw(5)<<(int)m.ise<<setw(5)<<ts;
        }
    }
    cout<<right<<setw(7)<<(int)s.grandTotal<<setw(8)<<fixed<<setprecision(2)<<s.percentage
        <<setw(5)<<s.grade<<setw(6)<<(s.overallPass?"PASS":"FAIL")<<setw(4)<<s.rank<<"\n";
    // ATKT line
    bool hasAtkt=!s.sem3Atkt.empty()||!s.prevAtkt.empty();
    if(hasAtkt){
        cout<<string(49,' ')<<"ATKT: ";
        for(auto &e:s.prevAtkt)
            cout<<"["<<e.semLabel<<": "<<trunc(e.subjectName,15)<<"]  ";
        if(!s.sem3Atkt.empty()){
            // Split sem3Atkt by space and show each in brackets
            string tmp=s.sem3Atkt; bool inWord=false; string word="";
            // sem3Atkt has full subject names separated by spaces
            // Better: split on known subject boundaries
            // Actually sem3Atkt = "Subject1 Subject2 " - subjects have spaces in names
            // So just show as one bracket
            cout<<"[Sem3: "<<trunc(s.sem3Atkt,35)<<"]";
        }
        cout<<"\n";
    }
    if(!s.mdmSubject.empty()||!s.oeSubject.empty()){
        cout<<string(49,' ');
        if(!s.mdmSubject.empty()) cout<<"MDM:"<<s.mdmSubject<<"  ";
        if(!s.oeSubject.empty())  cout<<"OE:"<<s.oeSubject;
        cout<<"\n";
    }
}

// ============================================================
//  MENU ACTIONS
// ============================================================
void fetchSingle(){
    cls();header();cout<<"  FETCH SINGLE STUDENT\n";drawLine('-',50);
    cout<<"\n  PRN number (10 digits). 0 = stop.\n\n";
    while(numStudents<MAX_S){
        string prn; cout<<"  PRN ["<<(numStudents+1)<<"]: "; cin>>prn; cin.ignore();
        if(prn=="0") break;
        bool valid=(prn.size()==10); for(char c:prn) if(!isdigit(c)){valid=false;break;}
        if(!valid){cout<<"  !! Must be 10 digits\n";continue;}
        bool dup=false;
        for(int i=0;i<numStudents;i++) if(students[i].prnNo==prn){cout<<"  !! Already added\n";dup=true;break;}
        if(dup) continue;
        Student s; if(fetchStudent(prn,s)){s.srNo=numStudents+1;students[numStudents++]=s;assignRanks();saveToFile();}
        cout<<"\n";
    }
    cout<<"\n  Total: "<<numStudents<<"\n"; wait();
}

void batchFetch(){
    cls();header();cout<<"  BATCH FETCH (prn_list.txt)\n";drawLine('-',50);
    cout<<"\n  Press Enter to start...";cin.get();
    ifstream f("prn_list.txt"); if(!f.is_open()){cout<<"\n  !! prn_list.txt not found!\n";wait();return;}
    vector<string> prns; string prn;
    while(getline(f,prn)){prn=trim(prn);if(!prn.empty())prns.push_back(prn);}
    f.close();
    cout<<"\n  Found "<<prns.size()<<" PRNs.\n\n";
    int ok=0,fail=0;
    for(const string &p:prns){
        if(numStudents>=MAX_S) break;
        bool dup=false; for(int i=0;i<numStudents;i++) if(students[i].prnNo==p){dup=true;break;}
        if(dup){cout<<"  SKIP: "<<p<<"\n";continue;}
        Student s; if(fetchStudent(p,s)){s.srNo=numStudents+1;students[numStudents++]=s;assignRanks();saveToFile();ok++;}else fail++;
        cout<<"\n";
    }
    cout<<"  Done! Success: "<<ok<<" | Failed: "<<fail<<"\n"; wait();
}

void viewAll(){
    cls();header(); if(numStudents==0){cout<<"  No records.\n";wait();return;}
    cout<<"  ALL RESULTS  (* below threshold  ! subject failed)\n\n";
    printHeader(); for(int i=0;i<numStudents;i++) printRow(students[i]);
    drawLine('=',tableWidth()); wait();
}

void analyze(){
    cls();header(); if(numStudents==0){cout<<"  No records.\n";wait();return;}
    cout<<"  RESULT ANALYSIS\n\n";
    int pass=0,fail=0,atktC=0,gc[5]={}; float totPct=0; int topIdx=0;
    for(int i=0;i<numStudents;i++){
        if(students[i].overallPass) pass++; else fail++;
        if(!students[i].sem3Atkt.empty()||!students[i].prevAtkt.empty()) atktC++;
        totPct+=students[i].percentage;
        if(students[i].percentage>students[topIdx].percentage) topIdx=i;
        switch(students[i].grade){case 'A':gc[0]++;break;case 'B':gc[1]++;break;case 'C':gc[2]++;break;case 'D':gc[3]++;break;default:gc[4]++;}
    }
    drawLine('-',45);
    cout<<left<<setw(22)<<"  Total Students"<<": "<<numStudents<<"\n";
    cout<<left<<setw(22)<<"  Sem-3 Passed"  <<": "<<pass<<"\n";
    cout<<left<<setw(22)<<"  Sem-3 Failed"  <<": "<<fail<<"\n";
    cout<<left<<setw(22)<<"  ATKT Pending"  <<": "<<atktC<<"\n";
    cout<<left<<setw(22)<<"  Class Average" <<": "<<fixed<<setprecision(2)<<(totPct/numStudents)<<"%\n";
    drawLine('-',45);
    cout<<"\n  GRADE DISTRIBUTION\n";drawLine('-',35);
    cout<<"  Grade A (>=75%) : "<<gc[0]<<"\n"<<"  Grade B (>=60%) : "<<gc[1]<<"\n"
        <<"  Grade C (>=50%) : "<<gc[2]<<"\n"<<"  Grade D (>=40%) : "<<gc[3]<<"\n"<<"  Grade F (<40%)  : "<<gc[4]<<"\n";
    drawLine('-',35);
    cout<<"\n  TOPPER\n";drawLine('-',45);
    cout<<"  Name  : "<<students[topIdx].name<<"\n"<<"  Seat  : "<<students[topIdx].seatNo<<"\n"
        <<"  PRN   : "<<students[topIdx].prnNo<<"\n"<<"  Total : "<<(int)students[topIdx].grandTotal<<" / 800\n"
        <<"  Pct   : "<<fixed<<setprecision(2)<<students[topIdx].percentage<<"%\n"<<"  Grade : "<<students[topIdx].grade<<"\n";
    drawLine('-',45);
    cout<<"\n  COMPLETE RANKING\n";drawLine('-',80);
    cout<<left<<setw(5)<<"Rank"<<setw(25)<<"Name"<<setw(8)<<"Seat"<<setw(8)<<"Total"<<setw(9)<<"Pct(%)"<<setw(6)<<"Grade"<<setw(7)<<"Sem3"<<"ATKT\n";
    drawLine('-',80);
    for(int r=1;r<=numStudents;r++)
        for(int i=0;i<numStudents;i++) if(students[i].rank==r){
            string atkt="-";
            if(!students[i].prevAtkt.empty()||!students[i].sem3Atkt.empty()){
                atkt="";
                for(auto &e:students[i].prevAtkt) atkt+="["+e.semLabel+"]"+trunc(e.subjectName,12)+" ";
                if(!students[i].sem3Atkt.empty()) atkt+="[Sem3]"+trunc(students[i].sem3Atkt,12);
            }
            cout<<left<<setw(5)<<r<<setw(25)<<students[i].name<<setw(8)<<students[i].seatNo
                <<setw(8)<<(int)students[i].grandTotal<<setw(9)<<fixed<<setprecision(2)<<students[i].percentage
                <<setw(6)<<students[i].grade<<setw(7)<<(students[i].overallPass?"PASS":"FAIL")<<"  "<<atkt<<"\n";
            break;
        }
    drawLine('-',80);
    cout<<"\n  SUBJECT-WISE TOPPER\n";drawLine('-',65);
    cout<<left<<setw(25)<<"Subject"<<setw(12)<<"Type"<<setw(22)<<"Topper"<<setw(6)<<"Marks"<<"Avg\n";
    drawLine('-',65);
    for(int j=0;j<numSubjects;j++){
        int best=0; for(int i=1;i<numStudents;i++) if(students[i].marks[j].total>students[best].marks[j].total) best=i;
        float sum=0; for(int i=0;i<numStudents;i++) sum+=students[i].marks[j].total;
        string dn=subjectSlot[j].empty()?subjectNames[j]:subjectSlot[j];
        cout<<left<<setw(25)<<trunc(dn,24)<<setw(12)<<typeStr(subjectType[j])
            <<setw(22)<<trunc(students[best].name,21)<<setw(6)<<(int)students[best].marks[j].total
            <<fixed<<setprecision(1)<<(sum/numStudents)<<"\n";
    }
    drawLine('-',65); wait();
}

void search(){
    cls();header(); if(numStudents==0){cout<<"  No records.\n";wait();return;}
    cout<<"  SEARCH (Roll / Seat / PRN / Name)\n\n  Enter: ";
    string q; 
    if(cin.peek()=='\n') cin.ignore();
    getline(cin,q);
    q=trim(q);
    string ql=q; transform(ql.begin(),ql.end(),ql.begin(),::tolower);
    bool found=false;
    for(int i=0;i<numStudents;i++){
        string nl=students[i].name; transform(nl.begin(),nl.end(),nl.begin(),::tolower);
        string seat=trim(students[i].seatNo);
        if(students[i].rollNo==q||seat==q||students[i].prnNo==q||nl.find(ql)!=string::npos){
            if(!found){cout<<"\n  RESULT\n\n";printHeader();}
            printRow(students[i]); found=true;
        }
    }
    if(!found) cout<<"\n  Not found.\n";
    else drawLine('=',tableWidth());
    wait();
}

void exportCSV(){
    cls();header(); if(numStudents==0){cout<<"  No records.\n";wait();return;}
    int ridx[MAX_S]; for(int i=0;i<numStudents;i++) ridx[i]=i;
    for(int a=0;a<numStudents-1;a++) for(int b=0;b<numStudents-1-a;b++)
        if(students[ridx[b]].rank>students[ridx[b+1]].rank){int t=ridx[b];ridx[b]=ridx[b+1];ridx[b+1]=t;}
    int pass=0,fail=0,atktC=0,gc[5]={}; float totPct=0; int top=0;
    for(int i=0;i<numStudents;i++){
        if(students[i].overallPass)pass++;else fail++;
        if(!students[i].sem3Atkt.empty()||!students[i].prevAtkt.empty())atktC++;
        totPct+=students[i].percentage;
        if(students[i].percentage>students[top].percentage) top=i;
        switch(students[i].grade){case 'A':gc[0]++;break;case 'B':gc[1]++;break;case 'C':gc[2]++;break;case 'D':gc[3]++;break;default:gc[4]++;}
    }
    // Try multiple save locations
    vector<string> paths = {
        "C:\\ExamAnalyzer\\result_analysis.csv",
        "C:\\Users\\Sharvari\\result_analysis.csv",
        "result_analysis.csv",
        "C:\\result_analysis.csv"
    };
    string savePath = "";
    ofstream f;
    for(const string &p : paths){
        f.clear();
        f.open(p, ios::out|ios::trunc);
        if(f.is_open()){ savePath=p; break; }
        f.close();
    }
    if(!f.is_open()){
        cout<<"  !! Cannot save file anywhere.\n";
        wait(); return;
    }
    cout<<"  Saving to: "<<savePath<<"\n";
    f<<"S.Y. Even Sem - Result Analysis\nDepartment of Computer Science (Data Science),Shivaji University Kolhapur\n\nRESULT TABLE\n";
    f<<"SR NO,ROLL NO,PRN NO,SEAT NO,NAME,MDM,OE";
    for(int j=0;j<numSubjects;j++){
        string sn=subjectSlot[j].empty()?subjectNames[j]:subjectSlot[j];
        for(char &c:sn) if(c==',') c=' ';
        int cols=(subjectType[j]==0)?5:(subjectType[j]==1)?4:3;
        f<<","<<sn; for(int k=1;k<cols;k++) f<<",";
    }
    f<<",TOTAL,PERCENTAGE,GRADE,SEM3,ATKT\n,,,,,,";
    for(int j=0;j<numSubjects;j++){
        if(subjectType[j]==0) f<<",MSE,ISE,ESE,TOT,RES";
        else if(subjectType[j]==1) f<<",POE,ISE,TOT,RES";
        else f<<",ISE,TOT,RES";
    }
    f<<",/800,%,,,\n";
    for(int i=0;i<numStudents;i++){
        Student &s=students[i];
        string atkt="";
        for(auto &e:s.prevAtkt) atkt+="["+e.semLabel+": "+e.subjectName+"] ";
        if(!s.sem3Atkt.empty()) atkt+="[Sem3: "+s.sem3Atkt+"]";
        if(atkt.empty()) atkt="-";
        for(char &c:atkt) if(c==',') c=';';
        f<<s.srNo<<","<<s.rollNo<<","<<s.prnNo<<","<<s.seatNo<<","<<s.name<<","<<s.mdmSubject<<","<<s.oeSubject;
        for(int j=0;j<numSubjects;j++){
            if(subjectType[j]==0) f<<","<<(int)s.marks[j].mse<<","<<(int)s.marks[j].ise<<","<<(int)s.marks[j].ese<<","<<(int)s.marks[j].total<<","<<(s.marks[j].passed?"P":"F");
            else if(subjectType[j]==1) f<<","<<(int)s.marks[j].poe<<","<<(int)s.marks[j].ise<<","<<(int)s.marks[j].total<<","<<(s.marks[j].passed?"P":"F");
            else f<<","<<(int)s.marks[j].ise<<","<<(int)s.marks[j].total<<","<<(s.marks[j].passed?"P":"F");
        }
        f<<","<<(int)s.grandTotal<<","<<fixed<<setprecision(2)<<s.percentage<<","<<s.grade<<","<<(s.overallPass?"PASS":"FAIL")<<","<<atkt<<"\n";
    }
    f<<"\nANALYSIS\nTotal Students,"<<numStudents<<"\nPassed,"<<pass<<",Failed,"<<fail<<",ATKT Pending,"<<atktC<<"\n";
    f<<"Class Average (%),"<<fixed<<setprecision(2)<<(totPct/numStudents)<<"\n";
    f<<"Grade A,"<<gc[0]<<",Grade B,"<<gc[1]<<",Grade C,"<<gc[2]<<",Grade D,"<<gc[3]<<",Grade F,"<<gc[4]<<"\n";
    f<<"\nCLASS TOPPER\nName,"<<students[top].name<<"\nSeat,"<<students[top].seatNo<<",PRN,"<<students[top].prnNo<<"\n";
    f<<"Total,"<<(int)students[top].grandTotal<<"/800,Pct,"<<students[top].percentage<<"%,Grade,"<<students[top].grade<<"\n";
    f<<"\nTOP 3 STUDENTS\nRANK,NAME,SEAT,TOTAL,PERCENTAGE,GRADE\n";
    for(int i=0;i<3&&i<numStudents;i++){Student &s=students[ridx[i]];f<<s.rank<<","<<s.name<<","<<s.seatNo<<","<<(int)s.grandTotal<<","<<fixed<<setprecision(2)<<s.percentage<<","<<s.grade<<"\n";}
    f<<"\nSUBJECT-WISE TOPPER\nSUBJECT,TYPE,TOPPER,SEAT,MARKS,CLASS AVG\n";
    for(int j=0;j<numSubjects;j++){
        int best=0; for(int i=1;i<numStudents;i++) if(students[i].marks[j].total>students[best].marks[j].total) best=i;
        float sum=0; for(int i=0;i<numStudents;i++) sum+=students[i].marks[j].total;
        string dn=subjectSlot[j].empty()?subjectNames[j]:subjectSlot[j];
        f<<dn<<","<<typeStr(subjectType[j])<<","<<students[best].name<<","<<students[best].seatNo<<","<<(int)students[best].marks[j].total<<","<<fixed<<setprecision(1)<<(sum/numStudents)<<"\n";
    }
    f.close();
    f.close();
    cout<<"  [OK] Saved: "<<savePath<<"\n";
    // Open file with default program (Excel)
    ShellExecuteA(NULL, "open", savePath.c_str(), NULL, NULL, SW_SHOW);
    wait();
}

// ============================================================
//  MAIN
// ============================================================
int main(){
    system("chcp 65001 >nul");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cls(); header();
    cout<<"  Checking pdftotext...\n"; setupPoppler();
    if(!g_popplerPath.empty()) cout<<"  [OK] Ready\n"; cout<<"\n";
    cout<<"  +-----------------------------+\n";
    cout<<"  |  1. Load existing data      |\n";
    cout<<"  |  2. Start fresh             |\n";
    cout<<"  +-----------------------------+\n  Choice: ";
    int sc; cin>>sc; cin.ignore();
    if(sc==1) loadFromFile();
    else{numStudents=0;numSubjects=0;cout<<"  Fresh start!\n";}
    cout<<"  Press Enter..."; cin.get();
    int ch; bool run=true;
    while(run){
        cls(); header();
        cout<<"  +------------------------------------------+\n";
        cout<<"  |  1. Fetch Single Student (PRN)           |\n";
        cout<<"  |  2. Batch Fetch (prn_list.txt)           |\n";
        cout<<"  |  3. View All Results                     |\n";
        cout<<"  |  4. Analyze Results                      |\n";
        cout<<"  |  5. Search (Roll/Seat/PRN/Name)          |\n";
        cout<<"  |  6. Export to CSV                        |\n";
        cout<<"  |  7. Exit                                 |\n";
        cout<<"  +------------------------------------------+\n";
        cout<<"\n  Students: "<<numStudents<<"  |  Choice: ";
        cin>>ch; cin.ignore();
        switch(ch){
            case 1:fetchSingle();break; case 2:batchFetch();break;
            case 3:viewAll();break;     case 4:analyze();break;
            case 5:search();break;      case 6:exportCSV();break;
            case 7:if(numStudents>0){cout<<"\n  Saving...\n";exportCSV();}cls();header();cout<<"  Goodbye!\n\n";run=false;break;
            default:cout<<"\n  Invalid.\n";cin.ignore(10000,'\n');cin.get();
        }
    }
    curl_global_cleanup(); return 0;
}
