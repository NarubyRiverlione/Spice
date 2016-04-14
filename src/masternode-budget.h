// Copyright (c) 2014-2016 The Dash Core developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef MASTERNODE_BUDGET_H
#define MASTERNODE_BUDGET_H

#include "main.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "base58.h"
#include "masternode.h"
#include "governance.h"
#include <boost/lexical_cast.hpp>
#include "init.h"

using namespace std;

extern CCriticalSection cs_budget;

// todo 12.1 - remove the unused
class CBudgetManager;
class CFinalizedBudget;
class CFinalizedBudget;
class CBudgetVote;
//class CBudgetProposal;
//class CBudgetProposal;
//class CBudgetVote;
class CTxBudgetPayment;
class CWalletTx;

static const CAmount BUDGET_FEE_TX = (5*COIN);
static const int64_t BUDGET_FEE_CONFIRMATIONS = 6;
static const int64_t BUDGET_VOTE_UPDATE_MIN = 60*60;

extern std::vector<CBudgetProposal> vecImmatureBudgetProposals;
extern std::vector<CFinalizedBudget> vecImmatureFinalizedBudgets;

extern CBudgetManager budget;

/**
* Finalized Budget Manager
* -------------------------------------------------------
* 
* This object is responsible for finalization of the budget system. It's built to be completely separate from
* the governance system, to eliminate any network differences.
*/

class CBudgetManager
{
private:

    //hold txes until they mature enough to use
    map<uint256, CTransaction> mapCollateral;
    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

public:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    
    map<uint256, CFinalizedBudget> mapFinalizedBudgets;

    std::map<uint256, CFinalizedBudget> mapSeenFinalizedBudgets;
    std::map<uint256, CBudgetVote> mapSeenFinalizedBudgetVotes;
    std::map<uint256, CBudgetVote> mapOrphanFinalizedBudgetVotes;
    //       parent hash       vote hash     vote
    std::map<uint256, std::map<uint256, CBudgetVote> > mapVotes;

    CBudgetManager() {
        mapFinalizedBudgets.clear();
    }

    void ClearSeen() {
        mapSeenFinalizedBudgets.clear();
        mapSeenFinalizedBudgetVotes.clear();
    }

    int CountFinalizedInventoryItems()
    {
        return mapSeenFinalizedBudgets.size() + mapSeenFinalizedBudgetVotes.size();
    }

    int sizeFinalized() {return (int)mapFinalizedBudgets.size();}

    void ResetSync(); 
    void MarkSynced();
    void Sync(CNode* node, uint256 nProp, bool fPartial=false);

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void NewBlock();

    CFinalizedBudget *FindFinalizedBudget(uint256 nHash);

    CAmount GetTotalBudget(int nHeight);
    
    std::vector<CFinalizedBudget*> GetFinalizedBudgets();

    bool IsBudgetPaymentBlock(int nBlockHeight);
    bool AddFinalizedBudget(CFinalizedBudget& finalizedBudget);
    void SubmitFinalBudget();
    bool HasNextFinalizedBudget();

    bool UpdateFinalizedBudget(CBudgetVote& vote, CNode* pfrom, std::string& strError);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void FillBlockPayee(CMutableTransaction& txNew, CAmount nFees);

    void CheckOrphanVotes();
    void Clear(){
        LOCK(cs);

        LogPrintf("Budget object cleared\n");
        mapFinalizedBudgets.clear();
        mapSeenFinalizedBudgets.clear();
        mapSeenFinalizedBudgetVotes.clear();
        mapOrphanFinalizedBudgetVotes.clear();
    }
    void CheckAndRemove();
    std::string ToString() const;
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(mapSeenFinalizedBudgets);
        READWRITE(mapSeenFinalizedBudgetVotes);
        READWRITE(mapOrphanFinalizedBudgetVotes);
        READWRITE(mapFinalizedBudgets);
        READWRITE(mapVotes);
    }

    void UpdatedBlockTip(const CBlockIndex *pindex);
    //# ----
};


class CTxBudgetPayment
{
    //# ----
public:
    uint256 nProposalHash;
    CScript payee;
    CAmount nAmount;

    CTxBudgetPayment() {
        payee = CScript();
        nAmount = 0;
        nProposalHash = uint256();
    }

    ADD_SERIALIZE_METHODS;

    //for saving to the serialized db
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CScriptBase*)(&payee));
        READWRITE(nAmount);
        READWRITE(nProposalHash);
    }
};



//
// Finalized Budget : Contains the suggested proposals to pay on a given block
//

class CFinalizedBudget
{

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    bool fAutoChecked; //If it matches what we see, we'll auto vote for it (masternode only)

public:
    bool fValid;
    std::string strBudgetName;
    int nBlockStart;
    std::vector<CTxBudgetPayment> vecBudgetPayments;
    map<uint256, CBudgetVote> mapVotes;
    uint256 nFeeTXHash;
    int64_t nTime;

    CFinalizedBudget();
    CFinalizedBudget(const CFinalizedBudget& other);

    void CleanAndRemove(bool fSignatureCheck);
    bool AddOrUpdateVote(CBudgetVote& vote, std::string& strError);
    double GetScore();
    bool HasMinimumRequiredSupport();

    bool IsValid(const CBlockIndex* pindex, std::string& strError, bool fCheckCollateral=true);

    std::string GetName() {return strBudgetName; }
    std::string GetProposals();
    int GetBlockStart() {return nBlockStart;}
    int GetBlockEnd() {return nBlockStart + (int)(vecBudgetPayments.size() - 1);}
    int GetVoteCount() {return (int)mapVotes.size();}
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    bool GetBudgetPaymentByBlock(int64_t nBlockHeight, CTxBudgetPayment& payment)
    {
        LOCK(cs);

        int i = nBlockHeight - GetBlockStart();
        if(i < 0) return false;
        if(i > (int)vecBudgetPayments.size() - 1) return false;
        payment = vecBudgetPayments[i];
        return true;
    }
    bool GetPayeeAndAmount(int64_t nBlockHeight, CScript& payee, CAmount& nAmount)
    {
        LOCK(cs);

        int i = nBlockHeight - GetBlockStart();
        if(i < 0) return false;
        if(i > (int)vecBudgetPayments.size() - 1) return false;
        payee = vecBudgetPayments[i].payee;
        nAmount = vecBudgetPayments[i].nAmount;
        return true;
    }

    //check to see if we should vote on this
    void AutoCheck();
    //total dash paid out by this budget
    CAmount GetTotalPayout();
    //vote on this finalized budget as a masternode
    void SubmitVote();

    //checks the hashes to make sure we know about them
    string GetStatus();

    uint256 GetHash(){
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << strBudgetName;
        ss << nBlockStart;
        ss << vecBudgetPayments;

        uint256 h1 = ss.GetHash();
        return h1;
    }

    ADD_SERIALIZE_METHODS;

    //for saving to the serialized db
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(LIMITED_STRING(strBudgetName, 20));
        READWRITE(nFeeTXHash);
        READWRITE(nTime);
        READWRITE(nBlockStart);
        READWRITE(vecBudgetPayments);
        READWRITE(fAutoChecked);

        READWRITE(mapVotes);
    }
};

#endif