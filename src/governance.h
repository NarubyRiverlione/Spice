// Copyright (c) 2014-2016 The Dash Core developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef GOVERANCE_H
#define GOVERANCE_H

#include "main.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "base58.h"
#include "masternode.h"
#include "governance-vote.h"
#include <boost/lexical_cast.hpp>
#include "init.h"

#include <stdio.h>
#include <string.h>

using namespace std;
extern CCriticalSection cs_budget;

// note: is there a reason these are static? 
//         http://stackoverflow.com/questions/3709207/c-semantics-of-static-const-vs-const 
static const CAmount BUDGET_FEE_TX = (5*COIN);
static const int64_t BUDGET_FEE_CONFIRMATIONS = 6;
static const int64_t BUDGET_VOTE_UPDATE_MIN = 60*60;
static const int64_t CONTRACT_ACTIVATION_TIME = 60*60*24*14;


class CGovernanceManager;
class CGovernanceObject;
class CBudgetVote;
class CNode;

// todo - 12.1 - change BUDGET_ to GOVERNANCE_ (cherry pick)
static const CAmount GOVERNANCE_FEE_TX = (5*COIN);
static const int64_t GOVERNANCE_FEE_CONFIRMATIONS = 6;
static const int64_t GOVERNANCE_UPDATE_MIN = 60*60;

extern std::vector<CGovernanceObject> vecImmatureBudgetProposals;
extern std::map<uint256, int64_t> askedForSourceProposalOrBudget;
extern CGovernanceManager governance;

//Check the collateral transaction for the budget proposal/finalized budget
extern bool IsCollateralValid(uint256 nTxCollateralHash, uint256 nExpectedHash, std::string& strError, int64_t& nTime, int& nConf, CAmount minFee);


//
// Governance Manager : Contains all proposals for the budget
//
class CGovernanceManager
{
private:

    //hold txes until they mature enough to use
    map<uint256, CTransaction> mapCollateral;
    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    int64_t nTimeLastDiff;

public:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    
    // keep track of the scanning errors I've seen
    map<uint256, CGovernanceObject> mapProposals;

    // todo - 12.1 - move to private for better encapsulation 
    std::map<uint256, CGovernanceObject> mapSeenMasternodeBudgetProposals;
    std::map<uint256, CBudgetVote> mapSeenMasternodeBudgetVotes;
    std::map<uint256, CBudgetVote> mapOrphanMasternodeBudgetVotes;
    //       parent hash       vote hash     vote
    std::map<uint256, std::map<uint256, CBudgetVote> > mapVotes;

    CGovernanceManager() {
        mapProposals.clear();
    }

    void ClearSeen() {
        mapSeenMasternodeBudgetProposals.clear();
        mapSeenMasternodeBudgetVotes.clear();
    }

    int CountProposalInventoryItems()
    {
        return mapSeenMasternodeBudgetProposals.size() + mapSeenMasternodeBudgetVotes.size();
    }

    int sizeProposals() {return (int)mapProposals.size();}

    // description: incremental sync with our peers
    // note: incremental syncing seems excessive, well just have clients ask for specific objects and their votes
    // note: 12.1 - remove
    //void ResetSync();
    //void MarkSynced();
    void Sync(CNode* node, uint256 nProp);

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void NewBlock();

    CGovernanceObject *FindProposal(const std::string &strName);
    CGovernanceObject *FindProposal(uint256 nHash);
    
    std::vector<CGovernanceObject*> GetAllProposals(int64_t nMoreThanTime);

    bool IsBudgetPaymentBlock(int nBlockHeight);
    bool AddProposal(CGovernanceObject& budgetProposal);
    bool UpdateProposal(CBudgetVote& vote, CNode* pfrom, std::string& strError);
    bool AddOrUpdateVote(CBudgetVote& vote, std::string& strError);
    bool PropExists(uint256 nHash);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void CleanAndRemove(bool fSignatureCheck);
    int CountMatchingVotes(int nVoteTypeIn, int nVoteOutcomeIn);

    void CheckOrphanVotes();
    void Clear(){
        LOCK(cs);

        LogPrintf("Budget object cleared\n");
        mapProposals.clear();
        mapSeenMasternodeBudgetProposals.clear();
        mapSeenMasternodeBudgetVotes.clear();
        mapOrphanMasternodeBudgetVotes.clear();
    }
    void CheckAndRemove();
    std::string ToString() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(mapSeenMasternodeBudgetProposals);
        READWRITE(mapSeenMasternodeBudgetVotes);
        READWRITE(mapOrphanMasternodeBudgetVotes);
        READWRITE(mapProposals);
        READWRITE(mapVotes);
    }

    void UpdatedBlockTip(const CBlockIndex *pindex);
    int64_t GetLastDiffTime() {return nTimeLastDiff;}
    void UpdateLastDiffTime(int64_t nTimeIn) {nTimeLastDiff=nTimeIn;}
};

/**
* Governance objects can hold any type of data
* --------------------------------------------
*
*/

class CGovernanceObjectRegister
{
private:
    int nType;
    std::string strReg;

public:
    CGovernanceObjectRegister(int nTypeIn, char* strRegIn)
    {
        nType = nTypeIn;
        strReg = strRegIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        //for syncing with other clients
        READWRITE(nType);
        READWRITE(LIMITED_STRING(strReg, 255));
    }
};

/**
* Generic Governance Object
*
*
*/

class CGovernanceObject
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    CAmount nAlloted;

public:

    uint256 nHashParent; //parent object, 0 is root
    int nRevision; //object revision in the system
    std::string strName; //org name, username, prop name, etc. 
    int64_t nTime; //time this object was created
    uint256 nFeeTXHash; //fee-tx
    
    // caching
    bool fValid;
    uint256 nHash;

    // Registers, these can be used for anything
    //   -- check governance wiki for correct usage
    std::map<int, CGovernanceObjectRegister> mapRegister;


    CGovernanceObject();
    CGovernanceObject(uint256 nHashParentIn, int nRevisionIn, std::string strNameIn, int64_t nTime, uint256 nFeeTXHashIn);

    void swap(CGovernanceObject& first, CGovernanceObject& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.strName, second.strName);
        swap(first.nHashParent, second.nHashParent);
        swap(first.nRevision, second.nRevision);
        swap(first.nTime, second.nTime);
        swap(first.nFeeTXHash, second.nFeeTXHash);
        swap(first.fValid, second.fValid);
        swap(first.nHash, second.nHash);     
        first.mapRegister.swap(second.mapRegister);
    }

    bool HasMinimumRequiredSupport();
    bool IsValid(const CBlockIndex* pindex, std::string& strError, bool fCheckCollateral=true);
    bool IsEstablished();
    bool NetworkWillPay();

    std::string GetName() {return strName; }

    // get vote counts on each outcome
    int GetAbsoluteYesCount(int nVoteOutcomeIn);
    int GetYesCount(int nVoteOutcomeIn);
    int GetNoCount(int nVoteOutcomeIn);
    int GetAbstainCount(int nVoteOutcomeIn);

    void CleanAndRemove(bool fSignatureCheck);
    void Relay();

    uint256 GetHash(){
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << strName;
        // ss << mapRegister;
        uint256 h1 = ss.GetHash();

        return h1;
    }

    /**
    *   AddRegister - Example usage:
    *   --------------------------------------------------------
    * 
    *   We don't really care what's in these, as long as the masternode network
    *   believes they're accurate. Otherwise the masternodes will vote them down 
    *   and we'll delete them from memory (fee-loss attack). 
    *
    *   - This system is designed to allow virtually any usage
    *   - No protocol changes are needed
    *   - Abuse will just be simply automatically deleted
    *   - Masternodes could read this data and do all sorts of things
    *
    *   Contractor: Mailing address, contact info (5 strings)
    *   Company: BitcoinAddress, UserHash, repeat (automate core team payments?)
    *   Contract: CAmount, nDenomination(int)
    *   Proposal:    
    *   MasternodePaymentsBlock: BlockStart, Masternode1, 2, 3... 
    *   Arbitration: UserId1, UserId2, TxHash, ContractHash
    
    */

    bool AddRegister(std::string& strError, int nTypeIn, std::string strIn)
    {
        if(strIn.size() > 64)
        {
            strError = "Too big.";
            return false;
        }

        char regbuff[64];
        strncpy(regbuff, strIn.c_str(), sizeof(regbuff));

        CGovernanceObjectRegister newRegister(nTypeIn, regbuff);
        mapRegister.insert(make_pair(1 , newRegister));
        return true;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        /**
        *   Store all major data items in serialization for other clients
        *   --
        *
        *
        *    uint256 nHashParent; //parent object, 0 is root
        *    int nRevision; //object revision in the system
        *    std::string strName; //org name, username, prop name, etc. 
        *    int64_t nTime; //time this object was created
        *    uint256 nFeeTXHash; //fee-tx
        *   
        *    // caching
        *    bool fValid;
        *    uint256 nHash;
        *
        *    // Registers, these can be used for anything
        *    //   -- check governance wiki for correct usage
        *    std::map<int, CGovernanceObjectRegister> mapRegister;
        *
        *
        */

        READWRITE(nHashParent);
        READWRITE(nRevision);
        READWRITE(LIMITED_STRING(strName, 64));
        READWRITE(nTime);
        READWRITE(nFeeTXHash);

        // todo - 12.1 - serialize map
        //READWRITE(mapRegister);
    }
};


#endif
