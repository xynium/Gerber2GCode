/*
 * HoleClass.cpp

 ____  ________.___._______  .___ ____ ___  _____
 \   \/  /\__  |   |\      \ |   |    |   \/     \
  \     /  /   |   |/   |   \|   |    |   /  \ /  \
  /     \  \____   /    |    \   |    |  /    Y    \
 /___/\  \ / ______\____|__  /___|______/\____|__  /
 \_/ \/              \/                    \/

 Mai 2018
 convertis les fichiers Gerber en Gcode


 Po les fichiers issu de kicad

 Traite le fichier .drl carte de percage


 TODO



 */

#include "PNGerber.hpp"
#include <string.h>
#include <vector>

using namespace std;

cHole::cHole(void) {
	iHasDrill = 1;
}

//Lit le fichier and build a vector of x y pos
// return 0 si OK or error code else
int cHole::ReadDrl(string filename) {

	string sLgnChamp[40];
	double daTool[NAPPERT]; // 30 Aperture a 7 champs
	char cL;
	string linebuffer;
	unsigned int iPos, iType, iOrder;
	int iTool;
	int iHasBeenTiTd;
	string sX, sY;
	string sXX, sYY;
	std::size_t found;

	std::ifstream infile(filename.c_str()); // open an input file stream
	iTool = 0;

	while (infile && getline(infile, linebuffer)) {
		if (linebuffer.length() == 0)
			continue;
		for (int ic = 0; ic < 10; ic++)
			sLgnChamp[ic] = "";
		iPos = 0; // num ordre char dans la lgn
		iType = 2; // type Change d'etat selon Fig / Num ou %
		iOrder = 0; // numero d'ordre du champ sur la lgn
		sLgnChamp[0] = "";
		while (iPos < linebuffer.size()) {   // parse
			cL = linebuffer[iPos];
			if ((cL == '%') && (iPos == 0)) {
				sLgnChamp[iOrder] = cL;
				//iOrder++;    le champ % ne sert a rien les commandes se distinguent seule
				sLgnChamp[iOrder] = "";
			} else {
				if (((cL < 58) && (cL > 47)) || (cL == '-') || (cL == '+')) {   // Numeric
					if (iType == 0) {
						iOrder++; // chg de champ
						sLgnChamp[iOrder] = "";
					}
					iType = 1;
					sLgnChamp[iOrder] += (cL);
				} else {
					if (iType == 1) {
						iOrder++; // chg de champ
						sLgnChamp[iOrder] = "";
					}
					iType = 0;
					sLgnChamp[iOrder] += cL;
				}
			}
			iPos++;
		} // fin du parsing dans le tab sLgnChamp les differants champs de la lgn en char ou num

		iHasBeenTiTd = 0;
		found = sLgnChamp[0].find("METRIC");
		if (found != std::string::npos) {
			iHasBeenTiTd = 1;
			std::cout << "'in drill file METRIC OK" << std::endl;
		}

		found = sLgnChamp[0].find("INCH");
		if (found != std::string::npos) {
			iHasBeenTiTd = 1;
			std::cout << std::endl << "Drill file in INCH no one use it" << std::endl;
			return 1;
		}

		if (sLgnChamp[0][0] == 'T') {     //Si T contien diam percage
			iHasBeenTiTd = 1;
			iTool = stoi(sLgnChamp[1]);
			if (sLgnChamp[2] == "C") { // declaration d'un diametre
				sX = sLgnChamp[3] + "." + sLgnChamp[5];
				daTool[iTool] = stod(sX);
			}
		}

		if ((sLgnChamp[0][0] == 'M') && (sLgnChamp[1][0] == '3') && (sLgnChamp[1][1] == '0')) {  //M30 fin drill file
			iHasBeenTiTd = 1;
			std::cout << std::endl << "End of drill file reached" << std::endl;
			return 0;
		}

		if (sLgnChamp[0][0] == 'X') {   // lgn de coordonée  enregistre les coord
			string sXN, sYN;
			Row tery;

			iHasBeenTiTd = 1;
			sX = sLgnChamp[1] + "." + sLgnChamp[3];
			sY = sLgnChamp[5] + "." + sLgnChamp[7];
			double dXb = stod(sX);
			double dYb = stod(sY);
			tery.push_back(dXb);
			if (MIRROR == 0)
				tery.push_back(dYb);
			else
				tery.push_back(-dYb);
			tery.push_back(daTool[iTool]);
			mdHole.push_back(tery);
		}

		if (iHasBeenTiTd == 0) {   // La lgn n'as pas été traitée pb
			std::cout << "Ligne drill non Traitée : " << linebuffer << std::endl;
			//return 1;
		}
	}
	return 1;
}

// find if a drill at pos dX dY return diamter or 0 if not
double cHole::DrillAtPos(double dX, double dY) {
	for (int in = 0; in < mdHole.size(); in++) {
		if ((fabs(mdHole[in][0] - dX) < DHOLETOL) && (fabs(mdHole[in][1] - dY) < DHOLETOL))
			return mdHole[in][2];
	}
	return 0.0;

}

