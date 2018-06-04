/*
 ____  ________.___._______  .___ ____ ___  _____
 \   \/  /\__  |   |\      \ |   |    |   \/     \
  \     /  /   |   |/   |   \|   |    |   /  \ /  \
  /     \  \____   /    |    \   |    |  /    Y    \
 /___/\  \ / ______\____|__  /___|______/\____|__  /
       \_/ \/              \/                    \/

 Avril 2018
 convertis les fichiers Gerber en Gcode


 Po les fichiers issu de kicad


 * PNGerber.hpp
 *

 *      Author: xynium
 */

#ifndef PNGERBER_HPP_
#define PNGERBER_HPP_

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <math.h>
#include <vector>

using namespace std;

//user def
#define ZDWN 0.1    // pen low level in mm
#define ZUP  5    // pen higth level in mm
#define PTDIAM 0.4   // in mm pen diameter 
#define CHVMT 0.1 // in mm recovering of 2 lines
#define MIRROR 0 // 1 si mirroir

//prog def
#define NAPPERT 60  // dimension tableau appert nombre d'appert supporté
#define DHOLETOL 0.0001   // tolerance dans la comparaison des position pour trouver si trou ou pas

typedef vector<double> Row; // One row of the matrix
typedef vector<Row> Matrix;

struct dPts {
	double dXp;
	double dYp;
	int iC;
};

//typedef  dPts tdPts;


class cHole {

private:
	Matrix mdHole;

public:
	int iHasDrill;  // =0 si le fichier drill has been corectly read
	int iMirror; // global parrameter defined

public:
	cHole(void);
	int ReadDrl(string filename);
	double DrillAtPos(double dX, double dY);
};




int pnFormat(string s, int i);
void PlotZone(Matrix mdPts);
dPts CalcCross(dPts PtA1, dPts PtA2, dPts PtB1, dPts PtB2);
double CalcPtDist(dPts PtA1, dPts PtA2, dPts PtB1);
void PlotTheTruc(double dX, double dY, double dXR, double dYR, double dTr, string sR);
void PlotSeg(double dXa, double dYa, double dXb, double dYb, double dXR);
void GetPCBLimit(string sf,dPts *dpO,dPts *dpE);
std::string format(const char* format, ...);

#endif /* PNGERBER_HPP_ */
