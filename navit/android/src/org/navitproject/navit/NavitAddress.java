package org.navitproject.navit;

public class NavitAddress {

        int mResultType;
        int mId;
        float mLat;
        float mLon;
        String mAddr;
        private String mAddrExtras;

        NavitAddress(int type, int id, float latitude, float longitude,
                     String address, String addrExtras) {
            mResultType = type;
            this.mId = id;
            mLat = latitude;
            mLon = longitude;
            mAddr = address;
            mAddrExtras = addrExtras;
        }

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
