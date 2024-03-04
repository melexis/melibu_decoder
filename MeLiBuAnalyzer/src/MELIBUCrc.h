#ifndef MELIBU_CRC_H
#define MELIBU_CRC_H

#include <LogicPublicTypes.h>

class MELIBUCrc
{
 public:
    MELIBUCrc();
    ~MELIBUCrc();

    void clear();
    U16 add( U8 byte );
    U16 result();

 private:
    U16 mCRC;
};

#endif // MELIBUCRC_H