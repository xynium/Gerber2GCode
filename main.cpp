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

 M02 est la fin de l'objet

 Le fonctionnement des D est assumé en G01


 TODO dans FS ne prend que si spec les chiffres apres la vigule


 */

/*
 *  Convert Gerber file to gcode so as to be ploted by a 3D printer with a pen
 Copyright (C) 2018  Lathuile Jean Pierre

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

 *
 */

#include "PNGerber.hpp"
#include <string.h>
#include <vector>
#include <string>
#include <cstdarg>
#include <memory>

using namespace std;
int iNXFrac, iNYFrac;
cHole cmHole;
std::ofstream outfile;
double dZDWN, dZUP, dPTDIAM, dCHVMT;  // global parrameter defined
//int iDeb[10]; // debug variable

int main(int argc, char **argv) {

	string filename;
	string sLgnChamp[40];
	string sAperture[NAPPERT][7]; // 30 Aperture a 7 champs
	char cL;
	string linebuffer;
	int iPos, iType, iOrder;
	int iApert, iCurApert;
	int iHasBeenTiTd;
	string sX, sY;
	double dXR, dYR, dX, dY;
	string sXX, sYY;
	Matrix mdEdg;
	int iInZone;
	int iMIRROR;
	int iNotInit;
	dPts *dpO, *dpE;

	std::cout
			<< "Gerber2GCode\n  Copyright (C) 2018  Lathuile Jean Pierre\nThis program comes with ABSOLUTELY NO WARRANTY; for details see licence file.\n This is free software, and you are welcome to redistribute it\n under certain conditions"
			<< linebuffer << std::endl;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s filename without .gbr if drl file present should have same name\n", argv[0]);
		fprintf(stderr, "\nFollowing options (after file name) :\n");
		fprintf(stderr, "    -M   -> Mirror default no\n");
		fprintf(stderr, "    -Px.x   -> x.x pen point diameter in mm default 0.4mm\n");
		fprintf(stderr, "    -Cx.x   -> x.x line recovering in mm default 0.1mm\n");
		fprintf(stderr, "    -Ux   ->  x pen up position mm default 5mm\n");
		fprintf(stderr, "    -Dx   ->  x pen down position mm default 0.1mm\n");
		return 1; //todo
	}
	filename = argv[1];
	//filename = "test";
	dZDWN = ZDWN;
	dZUP = ZUP;
	dPTDIAM = PTDIAM;
	dCHVMT = CHVMT;
	iMIRROR = MIRROR;
	for (int in = 2; in < argc; in++) {  // Scan parrameter
		if (argv[in][0] == '-') {
			if (argv[in][1] == 'M')
				iMIRROR = 1;
			if (argv[in][1] == 'P') {
				sX = argv[in];
				dPTDIAM = stod(sX.substr(2));
			}
			if (argv[in][1] == 'C') {
				sX = argv[in];
				dCHVMT = stod(sX.substr(2));
			}
			if (argv[in][1] == 'D') {
				sX = argv[in];
				dZDWN = stod(sX.substr(2));
			}
			if (argv[in][1] == 'U') {
				sX = argv[in];
				dZUP = stod(sX.substr(2));
			}
		}
	}

	outfile.open(filename + ".gcode"); // open an output file stream
	cmHole.iMirror = iMIRROR;
	cmHole.iHasDrill = cmHole.ReadDrl(filename + ".drl");

	sX = filename + ".gbr";
	std::ifstream infile(sX.c_str()); // open an input file stream

	std::cout << std::endl << "START" << std::endl;
	iApert = 0;
	// Entete gcode
	outfile << "G21" << std::endl;  // mm
	//outfile << "G28" << std::endl;  // home
	outfile << "G90" << std::endl;	//G90 ; use absolute coordinates
	outfile << "G1 Z" << dZUP << std::endl; // monte
	outfile << "G1 F600" << std::endl; //Set la vitesse mm/mn

	dpO = (dPts*) malloc(sizeof(dPts));
	dpE = (dPts*) malloc(sizeof(dPts));

	GetPCBLimit(filename, dpO, dpE);  // Cherche la dimension du pcb

	sX = format("PCB Dimension :  %f x %f  mm   ", dpE->dXp - dpO->dXp, dpE->dYp - dpO->dYp);

	std::cout << std::endl << sX << std::endl;

	sX = "0.0";  // point de depart normalment pas utilisé
	sY = "0.0";
	iNotInit = 1;
	iInZone = 0;
	//infile.seekg(ios_base::beg);

	while (infile && getline(infile, linebuffer)) {
		if (linebuffer.length() == 0)
			continue;
		iPos = 0; // num ordre char dans la lgn
		iType = 2; // type Change d'etat selon Fig / Num ou %
		iOrder = 0; // numero d'ordre du champ sur la lgn
		sLgnChamp[0] = "";
		while ((cL = linebuffer[iPos]) != '*') {   // parse
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
		if (cL == '*') { // Traite la ligne les 1 ou 2 premier char sLgnChamp definissent la commande
			if ((sLgnChamp[0][0] == 'F') && (sLgnChamp[0][1] == 'S')) {     //FS
				if ((sLgnChamp[0][2] == 'L') && (sLgnChamp[0][3] == 'A')) {
					iHasBeenTiTd = 1;
					pnFormat(sLgnChamp[1], 0);
					pnFormat(sLgnChamp[3], 1);
				} else {
					std::cout << std::endl << "FS non standart" << std::endl;
					return 1;
				}
			}
			if ((sLgnChamp[0][0] == 'M') && (sLgnChamp[0][1] == 'O')) {       //MO
				if ((sLgnChamp[0][2] == 'M') && (sLgnChamp[0][3] == 'M')) {
					iHasBeenTiTd = 1;
				} else {
					std::cout << std::endl << "MO non standart" << std::endl;
					return 1;
				}
			}
			if ((sLgnChamp[0][0] == 'L') && (sLgnChamp[0][1] == 'P')) {     //LP
				if ((sLgnChamp[0][2] == 'D')) {
					iHasBeenTiTd = 1;
				} else {
					std::cout << std::endl << "LP non standart" << std::endl;
					return 1;
				}
			}
			if ((sLgnChamp[0][0] == 'A') && (sLgnChamp[0][1] == 'D')) {        //AD
				iHasBeenTiTd = 1;
				sAperture[iApert][0] = "";     //sLgnChamp[0][2];  sous entend que il n'y a que des apperture D
				sAperture[iApert][0] += sLgnChamp[1];
				int ib = 1;
				for (int ia = 2; ia <= iOrder; ia++) {    // reste une virgule dans le champ 1
					sAperture[iApert][ib++] = sLgnChamp[ia];
					if (sLgnChamp[ia] == ".") {
						ib -= 2;
						sAperture[iApert][ib] += sLgnChamp[ia++];
						sAperture[iApert][ib++] += sLgnChamp[ia];
					}
				}
				iApert++;
			}
			if ((sLgnChamp[0][0] == 'M') && (sLgnChamp[1][0] == '0') && (sLgnChamp[1][1] == '2')) {  //M02
				iHasBeenTiTd = 1;
				std::cout << std::endl << "End of file reached" << std::endl;
				if (iInZone == 1)
					PlotZone(mdEdg);
				continue;
			}

			if (sLgnChamp[0][0] == 'G') {                                            // command G
				if ((sLgnChamp[1][0] == '0') && (sLgnChamp[1][1] == '4')) { // comment
					iHasBeenTiTd = 1;
				}
				if ((sLgnChamp[1][0] == '0') && (sLgnChamp[1][1] == '1')) { // interpolation lineaire non utilisé
					iHasBeenTiTd = 1;
				}
				if ((sLgnChamp[1][0] == '3') && (sLgnChamp[1][1] == '6')) { // Debut de zone    non traité
					iHasBeenTiTd = 1;
					if (iInZone == 1)
						PlotZone(mdEdg);
					iInZone = 1;
					mdEdg.clear();
				}
				if ((sLgnChamp[1][0] == '3') && (sLgnChamp[1][1] == '7')) { // fin zone
					iHasBeenTiTd = 1;
					if (iInZone == 1)
						PlotZone(mdEdg);
					iInZone = 1;      // Normalment 0 mais les zones semblent se suivre
					mdEdg.clear();
				}

			}

			if (sLgnChamp[0][0] == 'X') {   // lgn de coordonée  ECRITURE FICHIER GCODE
				unsigned int ia;
				int ic;
				string sXN, sYN;
				double dTr;

				for (ic = 0; ic < iOrder; ic++) { // met la virgule la longueur ne change pas -> le dernier chiffre saute
					if ((sLgnChamp[ic][0] == 'X') || (sLgnChamp[ic][0] == 'I')) {
						for (ia = sLgnChamp[ic + 1].size(); ia >= (sLgnChamp[ic + 1].size() - iNXFrac); ia--)
							sLgnChamp[ic + 1][ia + 1] = sLgnChamp[ic + 1][ia];
						sLgnChamp[ic + 1][ia + 1] = '.';
						sLgnChamp[ic + 1] = to_string(stod(sLgnChamp[ic + 1]) - dpO->dXp); // modif 4june additonne offset
						ic++;
					}

					if ((sLgnChamp[ic][0] == 'Y') || (sLgnChamp[ic][0] == 'J')) {
						for (ia = sLgnChamp[ic + 1].size(); ia >= (sLgnChamp[ic + 1].size() - iNYFrac); ia--)
							sLgnChamp[ic + 1][ia + 1] = sLgnChamp[ic + 1][ia];
						sLgnChamp[ic + 1][ia + 1] = '.';

						if (iMIRROR == 1)
							sLgnChamp[ic + 1] = to_string(dpE->dYp - stod(sLgnChamp[ic + 1])); // modif 4june mirroir new look
						else
							sLgnChamp[ic + 1] = to_string(stod(sLgnChamp[ic + 1]) - dpO->dYp); // modif 4june additonne offset

						ic++;
					}

					if ((ic == 4) && (iNotInit == 1)) { // Fixe l'origine au premier point lu
						iNotInit = 0;
						sX = sLgnChamp[1];
						sY = sLgnChamp[3];
					}

					////////////////////////////////PLOTE D01 ////////////////////////////////////////////
					if (sLgnChamp[ic][0] == 'D') {  // execute la cmd
						if ((sLgnChamp[ic + 1][0] == '0') && (sLgnChamp[ic + 1][1] == '1')) {
							if (iInZone == 0) { // Plote a track
								iHasBeenTiTd = 1;
								sXN = sLgnChamp[1];
								sYN = sLgnChamp[3];
								dXR = stod(sAperture[iCurApert][2]); // epaisseur de piste
								double dXa = stod(sX);
								double dXb = stod(sXN);
								double dYa = stod(sY);
								double dYb = stod(sYN);
								PlotSeg(dXa, dYa, dXb, dYb, dXR);
								sX = sXN;  // met a jour coord
								sY = sYN;
							} else { // polygone plot
								Row tery;
								sX = sLgnChamp[1];
								sY = sLgnChamp[3];
								double dXb = stod(sX);
								double dYb = stod(sY);
								tery.push_back(dXb);
								tery.push_back(dYb);
								mdEdg.push_back(tery);
								iHasBeenTiTd = 1;
							}
						}

						////////////////////////////////MOVE D02 ////////////////////////////////////////////
						if ((sLgnChamp[ic + 1][0] == '0') && (sLgnChamp[ic + 1][1] == '2')) {  // move
							if (iInZone == 0) {
								iHasBeenTiTd = 1;
								outfile << "G1 X" << sX << " Y" << sY << " Z" << dZUP << std::endl;
								sX = sLgnChamp[1];
								sY = sLgnChamp[3];
								outfile << "G1 X" << sLgnChamp[1] << " Y" << sLgnChamp[3] << std::endl;
							} else {  // Draw the aera move to begin pos
								Row tery;
								iHasBeenTiTd = 1;
								outfile << "G1 X" << sX << " Y" << sY << " Z" << dZUP << std::endl; // releve  le crayon
								sX = sLgnChamp[1];
								sY = sLgnChamp[3];
								double dXb = stod(sX);
								double dYb = stod(sY);
								tery.push_back(dXb);
								tery.push_back(dYb);
								mdEdg.push_back(tery);
								outfile << "G1 X" << sX << " Y" << sY << " Z" << dZUP << std::endl; // amenne le crayon  a la pos
							}
						}

						if ((sLgnChamp[ic + 1][0] == '0') && (sLgnChamp[ic + 1][1] == '3')) {    // Flash

							if (sAperture[iCurApert][1][0] == 'C') {  // Cercle
								iHasBeenTiTd = 1;
								// trace cercle le diam est dans sAperture[iCurApert][2]si ily un chmp 3 == X champs 4 donnne diam trou
								dTr = 0;
								dXR = stod(sAperture[iCurApert][2]) / 2.0;
								if (sAperture[iCurApert][3][0] == 'X')
									dTr = stod(sAperture[iCurApert][4]) / 2.0;

								outfile << "G1 X" << sX << " Y" << sY << " Z" << dZUP << std::endl; // amenne le crayon  a la pos
								sX = sLgnChamp[1];
								sY = sLgnChamp[3];
								dX = stod(sX);
								dY = stod(sY);
								PlotTheTruc(dX, dY, dXR, dXR, dTr, ""); // plotte
							}
							if (sAperture[iCurApert][1][0] == 'O') {  // Oblong
								iHasBeenTiTd = 1;
								// trace Oblong le X diam est dans sAperture[iCurApert][2]si ily un chmp 3 == X champs 4 donnne  dim y si champ 5 ==X champ 6 diam trou
								dTr = 0;
								dXR = stod(sAperture[iCurApert][2]) / 2.0;
								if (sAperture[iCurApert][3][0] == 'X')
									dYR = stod(sAperture[iCurApert][4]) / 2.0;
								else
									dYR = dXR;
								if (sAperture[iCurApert][5][0] == 'X')   // percage central
									dTr = stod(sAperture[iCurApert][6]) / 2.0;

								outfile << "G1 X" << sX << " Y" << sY << " Z" << dZUP << std::endl; // amenne le crayon  a la pos
								sX = sLgnChamp[1];
								sY = sLgnChamp[3];
								dX = stod(sX);
								dY = stod(sY);
								PlotTheTruc(dX, dY, dXR, dYR, dTr, ""); // plotte

							}
							if (sAperture[iCurApert][1][0] == 'R') {  // Rectangle
								iHasBeenTiTd = 1;
								// trace rectangle le X diam est dans sAperture[iCurApert][2]si ily un chmp 3 == X champs 4 donnne  dim y si champ 5 ==X champ 6 diam trou
								dTr = 0;
								dXR = stod(sAperture[iCurApert][2]) / 2.0;
								if (sAperture[iCurApert][3][0] == 'X')
									dYR = stod(sAperture[iCurApert][4]) / 2.0;
								else
									dYR = dXR;
								if (sAperture[iCurApert][5][0] == 'X')   // percage central
									dTr = stod(sAperture[iCurApert][6]) / 2.0;

								outfile << "G1 X" << sX << " Y" << sY << " Z" << dZUP << std::endl; // amenne le crayon  a la pos
								sX = sLgnChamp[1];
								sY = sLgnChamp[3];
								dX = stod(sX);
								dY = stod(sY);
								PlotTheTruc(dX, dY, dXR, dYR, dTr, "R"); // plotte
							}
							if (sAperture[iCurApert][1][0] == 'P') {  // Polygon
								//iHasBeenTiTd = 1;
							}
						}
					}

				}
			}

			if (sLgnChamp[0][0] == 'D') {                      // appert D repere son num dans le tab
				unsigned int ia;

				for (ia = 0; ia < NAPPERT; ia++) {
					if (sLgnChamp[1] == sAperture[ia][0]) {
						iHasBeenTiTd = 1;
						iCurApert = ia;
					}
				}

			}

			if (iHasBeenTiTd == 0) {   // La lgn n'as pas été traitée pb
				std::cout << "untreated line :  " << linebuffer << std::endl;
				//return 1;
			}
		}
	}

	free(dpO);
	free(dpE);

	outfile << "G1 Z" << dZUP << std::endl;  // return to org
	outfile << "G1 X0 Y0" << std::endl;

	std::cout << "DONE" << std::endl;
	outfile.close();
	infile.close();

	return (0);
}

// Decode la data de format
int pnFormat(string sF, int Pos) {
	int iT;

	iT = sF[1] - 48;
	if (Pos == 0)
		iNXFrac = iT;
	else
		iNYFrac = iT;
	return 1;
}

// Plote  zone G36 G37 le tableau de point est dans la var Matrix
// normaly pen up at at begining aera
void PlotZone(Matrix mdPts) {
	string sXX, sYY;
	double dYmax, dYmin, dXmax, dXmin;
	dPts PtR, PtA1, PtA2, PtB1, PtB2;
	int iUp, iZig;
	Matrix mvCross;
	int iAsToRedo;
	unsigned int iPrevN;

	dYmax = -100000.0;
	dYmin = 100000.0;
	dXmax = -100000.0;
	dXmin = 100000.0;
	iPrevN = 0;

	sXX = to_string(mdPts[0][0]);  //move au debut
	sYY = to_string(mdPts[0][1]);
	outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZUP << std::endl;

	for (unsigned int in = 0; in < mdPts.size(); in++) {  // Plote le contour et repere les Y min max
		if (dYmin > mdPts[in][1])
			dYmin = mdPts[in][1];
		if (dYmax < mdPts[in][1])
			dYmax = mdPts[in][1];
		if (dXmin > mdPts[in][0])
			dXmin = mdPts[in][0];
		if (dXmax < mdPts[in][0])
			dXmax = mdPts[in][0];
		sXX = to_string(mdPts[in][0]);
		sYY = to_string(mdPts[in][1]);
		outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZDWN << std::endl;
	}
	sXX = to_string(mdPts[0][0]);  // Reboucle l'aire
	sYY = to_string(mdPts[0][1]);
	outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZDWN << std::endl;
	outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZUP << std::endl;
	iZig = 0;
	iUp = 1;

	for (double dY = dYmin; dY < dYmax; dY += (dPTDIAM - dCHVMT)) {  // Remplie l'aire ligne axe X depart de Y=dYmax jusqu dYmin
		// Cherche les intersection avec les edges
		PtB1.dXp = dXmin;
		PtB2.dXp = dXmax;
		PtB1.dYp = dY;
		PtB2.dYp = dY;
		mvCross.clear();
		// for (unsigned int in = 0; in <7;in++)		iDeb[in]=0;

		for (unsigned int in = 0; in < mdPts.size(); in++) {  // Bcl tableau pts de croisement
			PtA1.dXp = mdPts[in][0];
			PtA1.dYp = mdPts[in][1];
			int in2 = ((in + 1) != mdPts.size()) ? in + 1 : 0;
			PtA2.dXp = mdPts[in2][0];
			PtA2.dYp = mdPts[in2][1];
			PtR = CalcCross(PtA1, PtA2, PtB1, PtB2);
			if (PtR.iC == 1) { //il y a cross  stoke les point de cross dans un tableau
				Row tery;
				tery.push_back(PtR.dXp);
				tery.push_back(PtR.dYp);
				mvCross.push_back(tery);  //new Row(dXa, dYa, dXb, dYb));
			}
			/*	if (PtR.iC > 2) { // po debug erreur dans le calcul des cross
			 iDeb[PtR.iC]++;
			 }*/
		}

		if (mvCross.size() > 0) {
			if (mvCross.size() % 2 != 0) {
				std::cout << "Odd number of crossing ploting zone at Y = " << dY << std::endl;
			} else {
				iAsToRedo = 1;
				while (iAsToRedo) {
					iAsToRedo = 0;
					for (unsigned int in = 0; in < mvCross.size() - 1; in++) {  // classe selon x croissant / decroissant
						if (((mvCross[in][0] > mvCross[in + 1][0]) && (iZig == 0)) || ((mvCross[in][0] < mvCross[in + 1][0]) && (iZig == 1))) { // inverse les elts
							Row rtmp = mvCross[in];
							mvCross[in] = mvCross[in + 1];
							mvCross[in + 1] = rtmp;
							iAsToRedo = 1;
						}
					}
				}
				if (iZig == 0)
					iZig = 1;
				else
					iZig = 0;

				if (mvCross.size() != iPrevN) { //Quand plusieurs zone si une disparrait relever
					iUp = 1;
					outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZUP << std::endl;
				}
				iPrevN = mvCross.size();
				//  trace la ligne en Zig Zag choisi un zig ou un zag
				for (unsigned int in = 0; in < mvCross.size(); in++) {
					sXX = to_string(mvCross[in][0]);
					sYY = to_string(mvCross[in][1]);
					if (iUp == 0) { //plote
						outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZDWN << std::endl;
						if (in == (mvCross.size() - 1))
							iUp = 2;
						else {
							outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZUP << std::endl;
							iUp = 1;
						}
					} else { //move
						if (iUp != 2)
							outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZUP << std::endl;
						outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZDWN << std::endl;
						iUp = 0;
					}
				}

				if (iUp == 0) {  // au cas ou
					outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZUP << std::endl;
					iUp = 1;
				}
			}
		}
	}
	if (iUp != 1)
		outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZUP << std::endl;
}

// Calcule le point de croisement de 2 lignes et retourne dans dPts.iC 1 si ce point est sur la ligne
//0 si pas de vroisement
// retour d'autre valeur erreur
// Ne compte pas les parralele car elle sont deja tracé dans le contour

dPts CalcCross(dPts PtA1, dPts PtA2, dPts PtB1, dPts PtB2) {
	dPts PtRet;
	double dDen;
	double dmX, dMX, dmY, dMY;

	dDen = (PtA1.dXp - PtA2.dXp) * (PtB1.dYp - PtB2.dYp) - (PtA1.dYp - PtA2.dYp) * (PtB1.dXp - PtB2.dXp);
	if (dDen != 0) {
		PtRet.dXp = ((PtA1.dXp * PtA2.dYp - PtA1.dYp * PtA2.dXp) * (PtB1.dXp - PtB2.dXp) - (PtB1.dXp * PtB2.dYp - PtB1.dYp * PtB2.dXp) * (PtA1.dXp - PtA2.dXp)) / dDen;
		PtRet.dYp = ((PtA1.dXp * PtA2.dYp - PtA1.dYp * PtA2.dXp) * (PtB1.dYp - PtB2.dYp) - (PtB1.dXp * PtB2.dYp - PtB1.dYp * PtB2.dXp) * (PtA1.dYp - PtA2.dYp)) / dDen;
	} else {    // les exeptions
		if (PtA1.dXp == PtA2.dXp) {
			if (PtA1.dYp == PtA2.dYp) { // erreur les pts A1 et A2 sont les meme
				PtRet.iC = 3;
				return PtRet;
			}
			if (PtB1.dXp == PtB2.dXp) {  // Normal implicite pour denominateur null lgn parralle
				PtRet.iC = 4;
				return PtRet;
			}

		}
		if (PtB1.dYp == PtB2.dYp) {
			if (PtB1.dXp == PtB2.dXp) {  //erreur les pts B1 et B2 sont les meme
				PtRet.iC = 5;
				return PtRet;
			}
			if (PtA1.dYp == PtA2.dYp) { //  Normal implicite lgn parralle
				PtRet.iC = 6;
				return PtRet;
			}
		}
	}

	dmX = min(PtB1.dXp, PtB2.dXp) * 0.999999;
	dMX = max(PtB1.dXp, PtB2.dXp) * 1.000001;
	dMY = max(PtA1.dYp, PtA2.dYp);
	dmY = min(PtA1.dYp, PtA2.dYp);

	if ((PtRet.dXp <= dMX) && (PtRet.dXp >= dmX) && (PtRet.dYp <= dMY) && (PtRet.dYp >= dmY))
		PtRet.iC = 1;
	else
		PtRet.iC = 0;

	return PtRet;
}

//Calcule distance ligne a un points
// Po respecter les percage dans les zones
// PtA1 PtA2 definissent la ligne PtB1 le point
double CalcPtDist(dPts PtA1, dPts PtA2, dPts PtB1) {
	double dDist;
	double dDen;

	dDen = sqrt(pow((PtA2.dYp - PtA1.dYp), 2) + pow((PtA2.dXp - PtA1.dXp), 2));
	if (dDen != 0) {
		dDist = fabs((PtA2.dYp - PtA1.dYp) * PtB1.dXp - (PtA2.dXp - PtA1.dXp) * PtB1.dYp + PtA2.dXp * PtA1.dYp - PtA2.dYp * PtA1.dXp) / dDen;
	} else {    // les exeptions		{ // erreur les pts A1 et A2 sont les meme
		dDist = NAN;
		return dDist;
	}

	return dDist;
}

//Plote un pad de percage oblong
//A l'entré le crayon est en haut a la precedente position
//a la sortie le crayon est up
//dX dY position du centre
//dXR dYR dimension en X et Y en distance depuis le centre
//dTr rayon percage si null aucun
// dessine un rectangle aveec les angles cassé si sR="R" plote un rectangle

void PlotTheTruc(double dX, double dY, double dXR, double dYR, double dTr, string sR) {
	string sXX, sYY;
	double dTmp;
	int iTp;

	if (cmHole.iHasDrill == 0) {    // repere si il y a un percage
		dTmp = cmHole.DrillAtPos(dX, dY);
		if (dTmp != 0)  // replace le rayon de percage
			dTr = dTmp / 2.0;
	}

	if (dPTDIAM / 2.0 <= dXR)
		dXR -= dPTDIAM / 2;
	else
		dXR = 0;
	if (dPTDIAM / 2.0 <= dYR)
		dYR -= dPTDIAM / 2;
	else
		dYR = 0;

	//if (dXR<0) dXR=0;   // modif 1/06 les pad de ci sont trop large
	//if (dYR<0) dYR=0;
	iTp = 0;

	while (iTp != 2) {
		dTmp = dXR > dYR ? dYR : dXR;
		//	dXD = dTmp / (1 + sqrt(2)); //ancien dXR
		//	dYD = dTmp / (1 + sqrt(2));  //ancien dYR ainsi le coin est le meme en X  et Y
		dTmp /= 2.0;
		if (sR == "R")
			dTmp = 0; // trace un rectangle

		sXX = to_string(dX + dXR);
		sYY = to_string(dY + dYR - dTmp);

		outfile << "G1 X" << sXX << " Y" << sYY << dZUP << std::endl;
		outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZDWN << std::endl; //dessine un octogone

		sYY = to_string(dY - dYR + dTmp);
		outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

		sXX = to_string(dX + dXR - dTmp);
		sYY = to_string(dY - dYR);
		outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

		sXX = to_string(dX - dXR + dTmp);
		outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

		sXX = to_string(dX - dXR);
		sYY = to_string(dY - dYR + dTmp);
		outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

		sYY = to_string(dY + dYR - dTmp);
		outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

		sXX = to_string(dX - dXR + dTmp);
		sYY = to_string(dY + dYR);
		outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

		sXX = to_string(dX + dXR - dTmp);
		outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

		sXX = to_string(dX + dXR);
		sYY = to_string(dY + dYR - dTmp);
		outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

		iTp = 2;
		if ((dXR - dPTDIAM + dCHVMT) > (dTr + dPTDIAM / 2.0)) {   // Normal diam trou pas crucial sinon ajuster po que la derniere soit au rayon dTr exact
			dXR -= dPTDIAM - dCHVMT;
			iTp--;
		}
		if ((dYR - dPTDIAM + dCHVMT) > (dTr + dPTDIAM / 2.0)) {
			dYR -= dPTDIAM - dCHVMT;
			iTp--;
		}

	}
}

// plote un segment de piste
//dXa dYa dXb dYb sont les coord des point extremite
//dXR l'epaisseur de la piste
//termine par un cercle et si percage a l'extremite evite le trou'

void PlotSeg(double dXa, double dYa, double dXb, double dYb, double dXR) {
	string sXX, sYY;
	double dTrkAng;
	double ddT, dRe;
	double dEffPst;
	int iNpass;
	double dz, dz2, dSgn, dAlt;

	//dXR *= 5.0;
	dSgn = 0.0;
	dAlt = 1.0;

	if (dPTDIAM < dXR) { // il vas y avoir plusieur passe

		dEffPst = dPTDIAM - dCHVMT;
		iNpass = ceil((dXR - dPTDIAM) / dEffPst);
		dEffPst = (dXR - dPTDIAM) / iNpass;    // corrige l'epaisseur de la piste eppaisseur effective

		ddT = dXR / 2.0 - dPTDIAM / 2.0;  //- ((double)iNpass / 2.0) * dEffPst  ;    //extreme par rapport a l'axe
		dRe = ddT; // dRe rayon du 1/2 cercle extremité
		dTrkAng = atan2((dYb - dYa), (dXb - dXa));
		while (dTrkAng < 0)
			dTrkAng += 2 * M_PI;

		if (cmHole.iHasDrill == 0) {    // repere si il y a un percage
			dz = cmHole.DrillAtPos(dXa, dYa);
			if (dz != 0) { // replace le pt dXa dYa pour ne pas boucher le trou
				dYa += (dz / 2.0 + dRe) * sin(dTrkAng);
				dXa += (dz / 2.0 + dRe) * cos(dTrkAng);
			}
			dz = cmHole.DrillAtPos(dXb, dYb);
			if (dz != 0) { // replace le pt dXa dYa pour ne pas boucher le trou
				dYb -= (dz / 2.0 + dRe) * sin(dTrkAng);
				dXb -= (dz / 2.0 + dRe) * cos(dTrkAng);
			}
		}

		dTrkAng = M_PI_2 + dTrkAng;  // perpendiculaire
		while (dTrkAng >= 2 * M_PI)
			dTrkAng -= 2 * M_PI;

		for (int iNp = 0; iNp <= iNpass; iNp++) {  // attention = rajoute une passe

			if (dRe > fabs(ddT))
				dz = sqrt(dRe * dRe - ddT * ddT);
			else
				dz = 0;
			dz2 = dz;

			if ((dTrkAng >= 0) && (dTrkAng < M_PI_2)) {  // x decroit y croit
				dz2 = -dz;
			}
			if ((dTrkAng >= M_PI_2) && (dTrkAng < M_PI)) { // piste croit x croit y ok
				dz = -dz;
				dSgn = M_PI;
			}
			if ((dTrkAng >= M_PI) && (dTrkAng < (3 * M_PI_2))) { //ok
				dz2 = -dz;
				dSgn = 0;
			}
			if ((dTrkAng >= (3 * M_PI_2)) && (dTrkAng < (4 * M_PI_2))) { //ok
				dz = -dz;
				dSgn = M_PI;
			}

			dz *= dAlt;
			sXX = to_string(dXa + ddT * cos(dTrkAng) - dz * sin(dTrkAng + dSgn));
			sYY = to_string(dYa + ddT * sin(dTrkAng) + dz * cos(dTrkAng + dSgn));
			outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZDWN << std::endl;
			dz2 *= dAlt;
			sXX = to_string(dXb + ddT * cos(dTrkAng) - dz2 * sin(dTrkAng + dSgn));
			sYY = to_string(dYb + ddT * sin(dTrkAng) + dz2 * cos(dTrkAng + dSgn));
			outfile << "G1 X" << sXX << " Y" << sYY << std::endl;

			ddT -= dEffPst;

			dXR = dXa;  // invertion org po pas avoir un zig zag entre passe
			dXa = dXb;
			dXb = dXR;
			dXR = dYa;
			dYa = dYb;
			dYb = dXR;
			dAlt = -dAlt;
			/*dXR = dz2;
			 dz2 = dz;
			 dz = dXR;
			 dXR = dA2;
			 dA2 = dA1;
			 dA1 = dXR;*/

		}
	} else {  // trace direc en 1 passe
		dRe = dPTDIAM / 2.0;
		dTrkAng = atan2((dYb - dYa), (dXb - dXa));
		while (dTrkAng < 0)
			dTrkAng += 2 * M_PI;

		if (cmHole.iHasDrill == 0) {    // repere si il y a un percage
			dz = cmHole.DrillAtPos(dXa, dYa);
			if (dz != 0) { // replace le pt dXa dYa pour ne pas boucher le trou
				dYa += (dz / 2.0 + dRe) * sin(dTrkAng);
				dXa += (dz / 2.0 + dRe) * cos(dTrkAng);
			}
			dz = cmHole.DrillAtPos(dXb, dYb);
			if (dz != 0) { // replace le pt dXa dYa pour ne pas boucher le trou
				dYb -= (dz / 2.0 + dRe) * sin(dTrkAng);
				dXb -= (dz / 2.0 + dRe) * cos(dTrkAng);
			}
		}

		sXX = to_string(dXa);
		sYY = to_string(dYa);
		outfile << "G1 X" << sXX << " Y" << sYY << " Z" << dZDWN << std::endl;
	}
	sXX = to_string(dXb);
	sYY = to_string(dYb);
	outfile << "G1 X" << sXX << " Y" << sYY << std::endl;  // positione a la fin de la piste si pas de move entre instruction

}

// retrouve les limites du pcb
//entree nom du fichier
//Sortie 2 points diagonale dpO minimal dpE maximal
void GetPCBLimit(string fs, dPts *dpO, dPts *dpE) {
	string linebuffer;
	int iPos, iType, iOrder;
	string sLgnChamp[40];
	char cL;
	double dT;

	string sX = fs + ".gbr";
	std::ifstream mifs(sX.c_str()); // open an input file stream

	dpO->dXp = 100000000;
	dpO->dYp = 100000000;
	dpE->dXp = -100000000;
	dpE->dYp = -100000000;

	while (mifs && getline(mifs, linebuffer)) {
		if (linebuffer.length() == 0)
			continue;
		iPos = 0; // num ordre char dans la lgn
		iType = 2; // type Change d'etat selon Fig / Num ou %
		iOrder = 0; // numero d'ordre du champ sur la lgn
		sLgnChamp[0] = "";
		while ((cL = linebuffer[iPos]) != '*') {   // parse
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

		if (cL == '*') { // Traite la ligne les 1 ou 2 premier char sLgnChamp definissent la commande
			if ((sLgnChamp[0][0] == 'F') && (sLgnChamp[0][1] == 'S')) {     //FS
				if ((sLgnChamp[0][2] == 'L') && (sLgnChamp[0][3] == 'A')) {
					pnFormat(sLgnChamp[1], 0);
					pnFormat(sLgnChamp[3], 1);
				}
			}

			if (sLgnChamp[0][0] == 'X') {   // lgn de coordonée  ECRITURE FICHIER GCODE
				unsigned int ia;
				int ic;
				string sXN, sYN;
				double dTr;

				for (ic = 0; ic < iOrder; ic++) { // met la virgule la longueur ne change pas -> le dernier chiffre saute
					if ((sLgnChamp[ic][0] == 'X') || (sLgnChamp[ic][0] == 'I')) {
						for (ia = sLgnChamp[ic + 1].size(); ia >= (sLgnChamp[ic + 1].size() - iNXFrac); ia--)
							sLgnChamp[ic + 1][ia + 1] = sLgnChamp[ic + 1][ia];
						sLgnChamp[ic + 1][ia + 1] = '.';
						ic++;
					}

					if ((sLgnChamp[ic][0] == 'Y') || (sLgnChamp[ic][0] == 'J')) {
						for (ia = sLgnChamp[ic + 1].size(); ia >= (sLgnChamp[ic + 1].size() - iNYFrac); ia--)
							sLgnChamp[ic + 1][ia + 1] = sLgnChamp[ic + 1][ia];
						sLgnChamp[ic + 1][ia + 1] = '.';
						ic++;
					}

					if (sLgnChamp[ic - 1] == "Y") {
						dT = stod(sLgnChamp[ic]);
						if (dT > dpE->dYp)
							dpE->dYp = dT;
						if (dT < dpO->dYp)
							dpO->dYp = dT;
						dT = stod(sLgnChamp[ic - 2]);
						if (dT > dpE->dXp)
							dpE->dXp = dT;
						if (dT < dpO->dXp)
							dpO->dXp = dT;

					}

				}
			}
		}
	}
	mifs.close();
}

std::string format(const char* format, ...) {
	va_list args;
	va_start(args, format);
#ifndef _MSC_VER
	size_t size = std::snprintf(nullptr, 0, format, args) + 1; // Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]);
	std::vsnprintf(buf.get(), size, format, args);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
#else
	int size = _vscprintf(format, args);
	std::string result(++size, 0);
	vsnprintf_s((char*)result.data(), size, _TRUNCATE, format, args);
	return result;
#endif
	va_end(args);
}
/*
 G1 X20.544472 Y20.077782 Z15
 G1 X25.000000 Y25.000000
 G1 X25.000000 Y25.000000 Z0.1
 G1 X-1.000000 Y-1.000000
 */

