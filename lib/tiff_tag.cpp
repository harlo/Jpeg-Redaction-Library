// tiff_tag.cpp: implementation of the TiffTag class.
//
//////////////////////////////////////////////////////////////////////

#include "tiff_tag.h"
#include "tiff_ifd.h"
#include "byte_swapping.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

namespace jpeg_redaction {
TiffTag::TiffTag(FILE *pFile, bool byte_swapping) : data_(NULL), subifd_(NULL)
{
    if (pFile == NULL)
      throw("NULL file");
    int iRV = fread(&tagid_, sizeof(short), 1, pFile);
    if (iRV != 1) throw("Can't read file");
    if (byte_swapping)
      ByteSwapInPlace(&tagid_, 1);
    iRV = fread(&type_, sizeof(short), 1, pFile);
    if (iRV != 1) throw("Can't read file");
    if (byte_swapping)
      ByteSwapInPlace(&type_, 1);
    iRV = fread(&count_, sizeof(unsigned int), 1, pFile);
    if (iRV != 1) throw("Can't read file");
    if (byte_swapping)
      ByteSwapInPlace(&count_, 1);

    unsigned int value = 0;
    iRV = fread(&value, sizeof(unsigned int), 1, pFile);
    if (iRV != 1) throw("Can't read file");
    int totallength = GetDataLength();
    if (totallength > 4 && byte_swapping) // If it's a pointer.
      ByteSwapInPlace(&value, 1);
#ifdef DEBUG
    printf("Read tag %d/%x, type %d count %d value %d/%0x "
	    "totallength %d\n",
	    tagid_, tagid_, type_, count_,
	   value, value, totallength);
#endif
    if (totallength < 0 || totallength > 1e8) {
      throw("totallength is broken");
    }
    // Some types are pointers that will be stored in an ifd.
    if (totallength <= 4 && ! (TagIsSubIFD()) ) {
    if (byte_swapping)
      ByteSwapInPlace((unsigned char *)&value,
		      4/LengthOfType(type_), LengthOfType(type_));
      loaded_ = true;
      data_ = new unsigned char [totallength];
      memcpy(data_, &value, totallength);
      valpointer_ = 0;
    } else {
      loaded_ = false;
      valpointer_ = value;
      // Too long to fit in the tag.
    }
}

TiffTag::~TiffTag() {
    delete subifd_;
    delete [] data_;
}

// Write out return 0 if the tag contains the data.
// If a pointer must be written, write a dummy, but return the offset
// so it can be corrected later.
int TiffTag::Write(FILE *pFile) const {
  int pointer_location = 0;
  int iRV = fwrite(&tagid_, sizeof(short), 1, pFile);
  if (iRV != 1) throw("Can't write file");
  iRV = fwrite(&type_, sizeof(short), 1, pFile);
  if (iRV != 1) throw("Can't write file");
  iRV = fwrite(&count_, sizeof(unsigned int), 1, pFile);
  if (iRV != 1) throw("Can't write file");

  const int totallength = GetDataLength();
  pointer_location = ftell(pFile);
  unsigned char raw[4];
  if (totallength <= 4 && data_)
    memcpy(raw, data_, totallength);
  iRV = fwrite(&raw, sizeof(unsigned char), 4, pFile);
  if (iRV != 4) throw("Can't write file");
  // Return where the pointer has to be written.
  if (totallength > 4 || TagIsSubIFD() ||
      tagid_ == tag_stripoffset|| tagid_ == tag_thumbnailoffset) {
    return pointer_location;
  } else {
    return 0;
  }
}

// Write out datablocks, returning the pointer. Return if no datablock.
int TiffTag::WriteDataBlock(FILE *pFile, int subfileoffset) {
  int totallength = count_ * LengthOfType(type_);
  //  int subfileoffset = 0; // TODO(aws) should this be non-zero?
  if (TagIsSubIFD()) {
    unsigned int zero = 0;
    valpointerout_ = subifd_->Write(pFile, zero, subfileoffset);
    return valpointerout_ + subfileoffset; // Will get subtracted later.
  }

  if (totallength > 4) {
    if (!loaded_)
      throw("Trying to write when data was never read");
    valpointerout_ = ftell(pFile);
    fwrite(data_, sizeof(unsigned char), totallength, pFile);
    return valpointerout_;
  }

  return 0;
}

// Load a type that didn't fit in the 4 bytes
int TiffTag::Load(FILE *pFile, unsigned int subfileoffset,
		   bool byte_swapping) {
  if (loaded_)
    return 0;
  if (pFile == NULL)
    throw("NULL file");
  int position = valpointer_ + subfileoffset;
  if (TagIsSubIFD()) {
    // IFDs use absolute position, normal tags are relative to subfileoffset.
    //if (tagid_ == tag_makernote)
    //    position = valpointer_;
    printf("Loading SUB IFD %0x at %d (%d + %d)", tagid_, position,
	  valpointer_, subfileoffset);

    subifd_ = new TiffIfd(pFile, position, true,
			    subfileoffset, byte_swapping);
    loaded_ = true;
    return 1;
  } else {
    int iRV = fseek(pFile, position, SEEK_SET);
    const int type_len = LengthOfType(type_);
    const int totallength = count_ * type_len;
    data_ = new unsigned char [totallength];
    iRV = fread(data_, sizeof(char), totallength, pFile);
    if (iRV  != totallength)
      throw("Couldn't read data block.");
    if (byte_swapping)
      ByteSwapInPlace(data_, count_, type_len);
    loaded_ = true;
    return totallength;
  }
}

void TiffTag::SetValOut(unsigned int val) {
  valpointerout_ = val;
}

bool TiffTag::TagIsSubIFD() const
{
  return(tagid_ == tag_exif || tagid_ == tag_gps || tagid_ == tag_gps ||
         /* tagid_ == tag_makernote || */ tagid_ == tag_interoperability);
}

void TiffTag::Print() const
{
      printf("0x%0x %dx%d (", tagid_, count_, LengthOfType(type_));
    TraceValue(4);
    printf(") ");

}

void TiffTag::TraceValue(int maxvals) const
{
  if (TagIsSubIFD()) {
    printf("IFD %d", valpointer_);
    return;
  }

  for(int i=0; i<maxvals && i< count_; ++i) {
    switch(type_) {
    case tiff_string:
      if (loaded_)
	printf("\"%s\"", GetStringValue());
      else
	printf("\"NOT LOADED\"");
      return;

    case tiff_float:
    case tiff_double:
    case tiff_rational:
    case tiff_urational:
      if (loaded_)
	printf("%f", GetFloatValue(i));
      else
	printf("Float not loaded");
      break;
    case tiff_bytes:
      if (loaded_)
	printf("0x%02x", GetUIntValue(i));
      else
	printf("bytes not loaded");
      break;
    case tiff_uint8:
    case tiff_uint16:
    case tiff_uint32:
      if (loaded_)
	printf("%u", GetUIntValue(i));
      else
	printf("Uint not loaded");
      break;
    case tiff_int8:
    case tiff_int16:
    case tiff_int32:
      if (loaded_)
	printf("%d", GetIntValue(i));
      else
	printf("int not loaded");
      break;
    }
    if (i<count_-1)
      printf(" ");
  }
}

int TiffTag::GetDataLength() const
{
  return (count_ * LengthOfType(type_));
}
}  // namespace jpeg_redaction
