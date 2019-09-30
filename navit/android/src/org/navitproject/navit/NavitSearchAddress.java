package org.navitproject.navit;


/**
 * Represents an address during a search.
 */
public class NavitSearchAddress {

    int mResultType;
    int mId;
    float mLat;
    float mLon;
    String mAddr;
    private String mAddrExtras;

    NavitSearchAddress(int type, int id, float latitude, float longitude,
                       String address, String addrExtras) {
        mResultType = type;
        this.mId = id;
        mLat = latitude;
        mLon = longitude;
        mAddr = address;
        mAddrExtras = addrExtras;
    }

    @Override
    public String toString() {
        if (mResultType == 2) { // housenumber
            return this.mAddr;
        }
        if (mResultType == 1) { // street
            return this.mAddr;
        }
        return (this.mAddr + " " + this.mAddrExtras);
    }
}
