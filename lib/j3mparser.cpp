/*
 * j3mparser.cpp
 *
 *  Created on: Aug 23, 2012
 *      Author: Harlo Holmes
 *
 */
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>

#include "jpeg.h"
#include "redaction.h"

using std::string;

void getMetadata(const char* filename) {
	unsigned int length = 0;
	const char* jpegFileName = filename;

	jpeg_redaction::Jpeg jpeg;
	if(!jpeg.LoadFromFile(jpegFileName, true))
		exit(1);

	const unsigned char *metadataBlob = jpeg.GetObscuraMetaData(&length);
	string out = reinterpret_cast<const char*>(metadataBlob);

	std::cout << out;
}

int main(int argc, char *argv[]) {
	if(argc != 2) {
		std::cout << "please init with a filename to open";
		exit(1);
	}

	std::cout << "file: " << argv[1] << "\n";
	getMetadata(argv[1]);
	return 0;
}


