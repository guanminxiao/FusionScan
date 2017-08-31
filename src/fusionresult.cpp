#include "fusionresult.h"
#include <sstream>
#include "editdistance.h"
#include "common.h"
#include <stdlib.h>

using namespace std;

FusionResult::FusionResult() {

}

FusionResult::~FusionResult() {
    
}

void FusionResult::addMatch(Match* m) {
    mMatches.push_back(m);
}

bool FusionResult::support(Match* m) {
    for(int i=0; i<mMatches.size(); i++) {
        if(supportSame(m, mMatches[i]))
            return true;
    }
    return false;
}

bool FusionResult::supportSame(Match* m1, Match* m2) {
    const int T=3;

    if( abs(m1->mLeftGP.position - m2->mLeftGP.position) > T)
        return false;
    if( abs(m1->mRightGP.position - m2->mRightGP.position) > T)
        return false;
    if( m1->mLeftGP.contig != m2->mLeftGP.contig)
        return false;
    if( m1->mRightGP.contig != m2->mRightGP.contig)
        return false;

    return true;
}

void FusionResult::calcFusionPoint() {
    if(mMatches.size() == 0)
        return ;

    // if we can find an exact match with 0 gap, then use it
    // else we use the mean one
    long leftTotal = 0;
    long rightTotal = 0;
    for(int i=0; i<mMatches.size(); i++) {
        Match* match = mMatches[i];
        if(match->mGap == 0){
            mLeftGP = match->mLeftGP;
            mRightGP = match->mRightGP;

            return ;
        }
        leftTotal += match->mLeftGP.position;
        rightTotal += match->mRightGP.position;
    }

    mLeftGP.contig = mMatches[0]->mLeftGP.contig;
    mLeftGP.position = leftTotal/(long)mMatches.size();
    mRightGP.contig = mMatches[0]->mRightGP.contig;
    mRightGP.position = rightTotal/(long)mMatches.size();

}
    
void FusionResult::calcUnique() {
    mUnique = 1;
    // since it is sorted, so just check every match with previous one
    for(int i=1; i<mMatches.size(); i++) {
        if(mMatches[i]->mReadBreak != mMatches[i-1]->mReadBreak ||
            mMatches[i]->mRead->length() != mMatches[i-1]->mRead->length())
            mUnique ++;
    }
}

bool FusionResult::isDeletion() {
    if(mLeftGP.contig == mRightGP.contig ) {
        if(mLeftGP.position>0 && mRightGP.position>0)
            return true;
        if(mLeftGP.position<0 && mRightGP.position<0)
            return true;
    }
    return false;
}

void FusionResult::makeTitle(vector<Fusion>& fusions) {
    stringstream ss;
    if(isDeletion())
        ss  << "Deletion: ";
    else
        ss  << "Fusion: ";
    ss << fusions[mLeftGP.contig].pos2str(mLeftGP.position) << "___";
    ss << fusions[mRightGP.contig].pos2str(mRightGP.position) ;
    ss << "  (total: " << mMatches.size() << ", unique:" << mUnique <<")";
    mTitle = ss.str();

    mLeftPos = fusions[mLeftGP.contig].pos2str(mLeftGP.position);
    mRightPos = fusions[mRightGP.contig].pos2str(mRightGP.position);

}

void FusionResult::makeReference(string& refL, string& refR) {
    int longestLeft = 0;
    int longestRight = 0;

    for(int i=0; i<mMatches.size(); i++) {
        if(mMatches[i]->mReadBreak + 1 > longestLeft)
            longestLeft = mMatches[i]->mReadBreak + 1;
        if(mMatches[i]->mRead->length() - (mMatches[i]->mReadBreak + 1) > longestRight)
            longestRight = mMatches[i]->mRead->length() - (mMatches[i]->mReadBreak + 1);
    }

    mLeftRef = getRefSeq(refL, mLeftGP.position - longestLeft + 1, mLeftGP.position);
    mRightRef = getRefSeq(refR, mRightGP.position, mRightGP.position + longestRight - 1);
}

string FusionResult::getRefSeq(string& ref, int start, int end) {
    // check start and end are in same strand
    if( (start>=0 && end<=0) || (start<=0 && end>=0) ) {
        return "";
    }

    // check the overflow
    if(abs(start)>=ref.length() || abs(end)>=ref.length())
        return "";

    int len = abs(end - start) + 1;

    if(start <0) {
        Sequence seq(ref.substr(-end, len));
        Sequence rcseq = ~seq;
        return rcseq.mStr;
    } else {
        return ref.substr(start, len);
    }
}

void FusionResult::adjustFusionBreak() {
    for(int i=0; i<mMatches.size(); i++) {
        int smallestED = 0xFFFF;
        int shift = 0;
        for(int s=-3; s<=3; s++) {
            int ed = calcED(mMatches[i], s);
            if(ed < smallestED) {
                smallestED = ed;
                shift = s;
            }
        }
        mMatches[i]->mReadBreak += shift;
        mMatches[i]->mLeftGP.position += shift;
        mMatches[i]->mRightGP.position += shift;
    }
}

int FusionResult::calcED(Match* m, int shift) {
    int readBreak = m->mReadBreak + shift;
    string seq = m->mRead->mSeq.mStr;
    int leftLen = readBreak + 1;
    int rightLen = seq.length() - leftLen;
    string leftSeq = seq.substr(0, leftLen);
    string rightSeq = seq.substr(leftLen, rightLen);

    int leftComp = min(20, min(leftLen, (int)mLeftRef.length()));
    int rightComp = min(20, min(rightLen, (int)mRightRef.length()));

    int leftED = edit_distance(leftSeq.substr(leftSeq.length() - leftComp, leftComp), mLeftRef.substr(mLeftRef.length() - leftComp, leftComp));
    int rightED = edit_distance(rightSeq.substr(0, rightComp), mRightRef.substr(0, rightComp));

    return leftED + rightED;
}

void FusionResult::print(vector<Fusion>& fusions) {
    cout << endl << "#" << mTitle << endl;
    for(int i=0; i<mMatches.size(); i++) {
        cout << ">" << i+1 << ", ";
        mMatches[i]->print();
    }
}