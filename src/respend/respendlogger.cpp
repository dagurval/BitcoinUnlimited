// Copyright (c) 2018 The Bitcoin developers
// Copyright (c) 2018-2019 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "respend/respendlogger.h"
#include "util.h"
#include "streams.h"
#include "utilstrencodings.h"

namespace respend
{
RespendLogger::RespendLogger() : tx2(nullptr), valid(false), newConflict(false)
{ }

bool RespendLogger::AddOutpointConflict(const COutPoint &,
    const CTxMemPool::txiter mempoolEntry,
    const CTransactionRef &pRespendTx,
    bool seen,
    bool isEquivalent)
{
    if (tx2 == nullptr)
        tx2 = pRespendTx.get();

    if (!isEquivalent) {
        newConflict = newConflict || !seen;
        tx1s.insert(mempoolEntry);
    }

    // Keep gathering conflicting transactions
    return true;
}

bool RespendLogger::IsInteresting() const
{
    // Logging never triggers full tx validation
    return false;
}

void RespendLogger::Trigger() {
    if (!valid || tx1s.empty() || !newConflict)
        return;

    // Log tx2
    LOG(RESPEND, "Respend tx2: %s\n", tx2->GetHash().ToString());
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << *tx2;
    LOG(RESPEND, "Respend tx2 hex: %s\n",HexStr(ssTx.begin(), ssTx.end()));

    // Log tx1s
    for (const auto& tx1Entry : tx1s) {
        const CTransaction& tx1 = tx1Entry->GetTx();
        LOG(RESPEND, "Respend tx1: %s %s %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", tx1Entry->GetTime()), tx1.GetHash().ToString(), tx2->GetHash().ToString());
        CDataStream sspTx(SER_NETWORK, PROTOCOL_VERSION);
        sspTx << tx1;
        LOG(RESPEND, "Respend tx1 hex: %s\n",HexStr(sspTx.begin(), sspTx.end()));
    }
}

} // ns respend
