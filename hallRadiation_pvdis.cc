#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <math.h>

#include <TH1D.h>
#include <TH1.h>
#include <TNtuple.h>
#include <TFile.h>
#include <TDirectory.h>
#include "radDamage.hh"

using namespace std;

//log X axis histograms
void niceLogBins(TH1*);
//energy/neil/mrem
//photons/electron/neutrons
vector<vector<vector<TH1D*> > > hTotal, hAvg, valAvg;
vector<vector<vector<vector<int> > > > intAvg;
const int nBins=200;
TH1D *hSummary[3];//energy, neil, mRem
void UpdateMeans();

TFile *fout;

void Initialize();
void Summarize();
void FinalizeAvg();
void WriteOutput();

int processInput(int,char**);
string suffix;
string finNm("0");
int nAvg(100000000);
//vector<int> detNr={1006};
vector<int> detNr={1001, 1002, 1003, 1004, 1005, 1006, 1007, 1101, 1102, 2101, 2105, 2110, 2112, 3120, 3121, 3201, 2401, 2411};

long currentEv(0),prevEv(0),processedEv(0);
void ProcessOne(string);
radDamage radDmg;

int main(int argc, char **argv){

  int inputRes = processInput(argc,argv);
  if(inputRes)
    return inputRes;

  string foutNm;
  if(suffix!="")
    foutNm = suffix + "_hallRad.root";
  else
    foutNm = Form("%s_hallRad.root",finNm.substr(0,finNm.find(".")).c_str());
  fout=new TFile(foutNm.c_str(),"RECREATE");

  Initialize();
  if ( finNm.find(".root") < finNm.size() ){
    ProcessOne(finNm);
  }else{
    ifstream ifile(finNm.c_str());
    string data;
    while(ifile>>data){
      cout<<" processing: "<<data<<endl;
      ProcessOne(data);
    }
  }
  cout<<"Processed a total of "<<processedEv<<endl;
  FinalizeAvg();
  Summarize();
  WriteOutput();
  cout<<"written"<<endl;
  return 0;
}

void ProcessOne(string fnm){
  TFile *fin=new TFile(fnm.c_str(),"READ");
  if(!fin->IsOpen()){
    cout<<"Problem: can't find file: "<<fnm<<endl;
    fin->Close();
    delete fin;
    return;
  }

  Float_t type, volume, evNr;
  Float_t Edeposit,kinE;
  Float_t x0,y0,z0,xd,yd,zd;
  Float_t pdgID;
  TNtuple *t = (TNtuple*)fin->Get("geant");
  t->SetBranchAddress("type",&type);
  t->SetBranchAddress("volume",&volume);
  t->SetBranchAddress("x",&xd);
  t->SetBranchAddress("y",&yd);
  t->SetBranchAddress("z",&zd);
  t->SetBranchAddress("x0",&x0);
  t->SetBranchAddress("y0",&y0);
  t->SetBranchAddress("z0",&z0);
  t->SetBranchAddress("ev_num",&evNr);
  t->SetBranchAddress("PDGid",&pdgID);
  t->SetBranchAddress("Edeposit",&Edeposit);
  t->SetBranchAddress("kineE",&kinE);

  long nEntries= t->GetEntries();
  float currentProc=1,procStep=10;
  int nDet=detNr.size();
  for(long i=0;i<nEntries;i++){
    //if( float(i+1)/nEntries*100 > 51) continue;
    t->GetEntry(i);
    if( float(i+1)/nEntries*100 > currentProc){
      cout<<"at tree entry\t"<<i<<"\t"<< float(i+1)/nEntries*100<<" %"<<endl;
      currentProc+=procStep;
    }

    currentEv += evNr - prevEv;
    prevEv = evNr;
    if( currentEv > nAvg ){
      currentEv=currentEv - nAvg;
      UpdateMeans();
    }

    int nHist(-1);
    for(int id=0;id<nDet;id++)
      if(volume==detNr[id]){
        nHist=id;
        break;
      }
    if(nHist==-1) continue;

    int nPart(-1);
    if( abs(pdgID) == 11 ) nPart=1;
    else if( pdgID == 2112 ) nPart=2;
    else if( pdgID == 22 ) nPart=0;
    else continue;

    //electrons directly from the gun
    if(z0 == -17720) continue;

    //if(z0>= 26000) continue;

    double energy(-1);
    if( (volume < 2000 && volume > 1000) || volume==3201 ) //Kryptonite detectors or the o-ring
      energy = Edeposit;
    else //vacuum detectors
      energy = kinE;

    //logX(Energy)
    hTotal[nHist][nPart][0]->Fill(energy);
    valAvg[nHist][nPart][0]->Fill(energy);
    //linX(Energy)
    hTotal[nHist][nPart][3]->Fill(energy);
    valAvg[nHist][nPart][3]->Fill(energy);

    double val(-1);
    val = radDmg.getNEIL(pdgID,energy,0);
    if(val!=-999){
      //logX(Energy)
      hTotal[nHist][nPart][1]->Fill(energy,val);
      valAvg[nHist][nPart][1]->Fill(energy,val);
      //linX(Energy)
      hTotal[nHist][nPart][4]->Fill(energy,val);
      valAvg[nHist][nPart][4]->Fill(energy,val);
    }

    val = radDmg.getMREM(pdgID,energy,0);
    if(val!=-999){
      hTotal[nHist][nPart][2]->Fill(energy,val);
      valAvg[nHist][nPart][2]->Fill(energy,val);
    }
  }

  processedEv += ceil(prevEv/1000.)*1000;
  prevEv = 0;
  fin->Close();
  delete fin;
}

void UpdateMeans(){
  int nDet=detNr.size();
  for(int id=0;id<nDet;id++){
    for(int ip=0;ip<3;ip++){
      for(int idmg=0;idmg<5;idmg++){
        int nbins= hAvg[id][ip][idmg]->GetXaxis()->GetNbins();
        for(int ib=1;ib<=nbins;ib++){
          double val = valAvg[id][ip][idmg]->GetBinContent(ib)/nAvg;
          valAvg[id][ip][idmg]->SetBinContent(ib,0);

          intAvg[id][ip][idmg][ib]++;
          double currentMean = hAvg[id][ip][idmg]->GetBinContent(ib);
          double currentVar  = hAvg[id][ip][idmg]->GetBinError(ib);

          double delta   = val - currentMean;
          double newMean = currentMean + delta/intAvg[id][ip][idmg][ib];
          double delta2  = val - newMean;
          double newVar  = currentVar + delta*delta2;

          hAvg[id][ip][idmg]->SetBinContent(ib,newMean);
          hAvg[id][ip][idmg]->SetBinError(ib,newVar);
        }
      }
    }
  }
}

void Initialize(){
  string hPnm[3]={"g","e","n"};
  string type[5]={"enerLogX","neilLogX","mRemLogX","enerLinX","neilLinX"};

  int nDet=detNr.size();
  for(int id=0;id<nDet;id++){
    vector<vector<TH1D*> > dt1,da1,dv1;
    intAvg.push_back(vector<vector<vector<int> > >(3, vector<vector<int> >(5, vector<int>(nBins))));
    for(int ip=0;ip<3;ip++){
      int nrBins=nBins;
      vector<TH1D*> dt2,da2,dv2;
      for(int idmg=0;idmg<5;idmg++){
        if(idmg>=3) nrBins=200;
        TH1D *h=new TH1D(Form("ht_%d_%s_%s",detNr[id],hPnm[ip].c_str(),type[idmg].c_str()),
                         Form("Total hits for det %d| part: %s| %s; energy [MeV]",
                              detNr[id],hPnm[ip].c_str(),type[idmg].c_str()),
                         nrBins,-8,3.8);

        TH1D *a=new TH1D(Form("ha_%d_%s_%s",detNr[id],hPnm[ip].c_str(),type[idmg].c_str()),
                         Form("Hits/(%d ev) hits for det %d| part: %s| %s; energy [MeV]",
                              nAvg,detNr[id],hPnm[ip].c_str(),type[idmg].c_str()),
                         nrBins,-8,3.8);

        //dummy histograms
        TH1D *v=new TH1D(Form("hv_%d_%s_%s",detNr[id],hPnm[ip].c_str(),type[idmg].c_str()),
                         Form("Hits/(%d ev) hits for det %d| part: %s| %s; energy [MeV]",
                              nAvg,detNr[id],hPnm[ip].c_str(),type[idmg].c_str()),
                         nrBins,-8,3.8);

        if(idmg>=3){
          double xBins[201];
          for(int i=0;i<=40;i++){
            xBins[i]     = i*(0.000001)/40;
            xBins[40+i]  = 0.000001 + i*(0.001-0.000001)/40;
            xBins[80+i]  = 0.001 + i*(1-0.001)/40;
            xBins[120+i] = 1 + i*(10 - 1)/40;
            xBins[160+i] = 10 + i*(6300 - 10)/40;
          }
          h -> GetXaxis() -> Set(200,xBins);
          a -> GetXaxis() -> Set(200,xBins);
          v -> GetXaxis() -> Set(200,xBins);
        }else{
          niceLogBins(h);
          niceLogBins(a);
          niceLogBins(v);
        }

        dt2.push_back(h);
        da2.push_back(a);
        dv2.push_back(v);
      }
      dt1.push_back(dt2);
      da1.push_back(da2);
      dv1.push_back(dv2);
    }
    hTotal.push_back(dt1);
    hAvg.push_back(da1);
    valAvg.push_back(dv1);
  }
  for(int i=0;i<3;i++){
    hSummary[i]=new TH1D(Form("hSummary_%s",type[i].c_str()),
                         Form("summary histogram per electron on target| %s",type[i].c_str()),
                         detNr.size()*2,0,detNr.size()*2);
    for(int ib=1;ib<=int(detNr.size());ib++){
      hSummary[i]->GetXaxis()->SetBinLabel(2*ib-1,Form("%d Tot",detNr[ib-1]));
      hSummary[i]->GetXaxis()->SetBinLabel(2*ib ,Form("%d Avg",detNr[ib-1]));
    }
  }
}

void Summarize(){
  for(int idmg=0;idmg<3;idmg++){
    for(int idet=1;idet<=int(detNr.size());idet++){
      double tot(0),avg(0),sig(0);
      for(int ipart=0;ipart<3;ipart++){
        tot+=hTotal[idet-1][ipart][idmg]->Integral();
        for(int ib=1;ib<=hAvg[idet-1][ipart][idmg]->GetXaxis()->GetNbins();ib++){
          avg += hAvg[idet-1][ipart][idmg]->GetBinContent(ib);
          sig = sqrt(pow(hAvg[idet-1][ipart][idmg]->GetBinError(ib),2) + pow(sig,2));
        }
      }
      hSummary[idmg]->SetBinContent(2*idet-1,tot/processedEv);
      hSummary[idmg]->SetBinError(2*idet-1,0);
      hSummary[idmg]->SetBinContent(2*idet,avg);
      hSummary[idmg]->SetBinError(2*idet,sig);
    }
  }
}

void FinalizeAvg(){
  int nDet=detNr.size();
  for(int id=0;id<nDet;id++){
    for(int ip=0;ip<3;ip++){
      for(int idmg=0;idmg<5;idmg++){
        int nbins = hAvg[id][ip][idmg]->GetXaxis()->GetNbins();
        for(int ib=1;ib<=nbins;ib++){
          double d(0);
          if(intAvg[id][ip][idmg][ib]>=2)
            d = sqrt(hAvg[id][ip][idmg]->GetBinError(ib)/(intAvg[id][ip][idmg][ib]-1))/sqrt(intAvg[id][ip][idmg][ib]);

          if(d==0){
            hAvg[id][ip][idmg]->SetBinError(ib,0);
            hAvg[id][ip][idmg]->SetBinContent(ib,0);
          }
          else{
            hAvg[id][ip][idmg]->SetBinError(ib, d);
          }
        }
      }
    }
  }
}
void WriteOutput(){
  fout->cd();
  for(int i=0;i<3;i++)
    hSummary[i]->Write();
  int nDet=detNr.size();

  for(int id=0;id<nDet;id++){
    fout->cd();
    fout->mkdir(Form("Det_%d",detNr[id]));
    fout->cd(Form("Det_%d",detNr[id]));
    for(int ip=0;ip<3;ip++){
      for(int idmg=0;idmg<5;idmg++){
        hTotal[id][ip][idmg]->Write();
        hAvg[id][ip][idmg]->Write();
      }
    }
  }
  fout->Close();
}

void niceLogBins(TH1*h)
{
  TAxis *axis = h->GetXaxis();
  int bins = axis->GetNbins();

  double from = axis->GetXmin();
  double to = axis->GetXmax();
  double width = (to - from) / bins;
  double *new_bins = new double[bins + 1];

  for (int i = 0; i <= bins; i++) {
    new_bins[i] = pow(10, from + i * width);
  }
  axis->Set(bins, new_bins);
  delete new_bins;
}

int processInput(int argc, char **argv){
  if( argc == 1 || (strcmp("--help",argv[1])==0)){
    cout << "Usage:\n$build/hallRad --infile [file.name] <other options>\n";
    cout << "Options:\n\t--help : print this usage guide\n";
    cout << "\t--infile <file.name>: specify prexSim output file to process. Can be root file or list of paths to rootfiles. \n";
    cout << "\t--default : process a set of default detectors and use default averaing\n";
    cout << "\t\tdefault averging: "<<nAvg<<endl;
    cout << "\t\tdefault detectors:";
    for(auto &element : detNr)
      cout<<"\t"<<element;
    cout<<endl;
    cout << "\t--avgOverN <number> : int used as number of events to average over for uncertainty\n";
    cout << "\t--detList <Ndetector> : list of detectors that you want to process\n";
    cout << "\t--suffix <name> : suffix for output file. can contain path to a particular folder\n";
    return 1;
  }

  int defaultFlag(0);
  for(int i=1;i<argc;i++){
    if(strcmp("--infile",argv[i])==0){
      finNm = argv[i+1];
    }else if(strcmp("--avgOverN",argv[i])==0){
      nAvg = atoi(argv[i+1]);
    }else if(strcmp("--default",argv[i])==0){
      defaultFlag=1;
    }else if(strcmp("--suffix",argv[i])==0){
      suffix=argv[i+1];
    }else if(strcmp("--detList",argv[i])==0){
      defaultFlag = -1;
      detNr.clear();
      for(int elem=i+1;elem<argc;elem++){
	int det = atoi(argv[elem]);
	if(det<1000 || det>12000) break;
	detNr.push_back(det);
      }
    }
  }

  if(finNm == "0"){
    cout << "you have to provide an input file! quitting" <<endl;
    return 2;
  }else if(defaultFlag == 1){
    cout << "using default detectors and averaging\n";
  }else if(defaultFlag == 0){
    cout << "default not specified and no detector list given. using default.\n";
  }

  cout<<"Processing for the following detectors:\n";
  for(auto &element : detNr)
    cout<<element<<"\t";
  cout<<endl;

  return 0;
}
