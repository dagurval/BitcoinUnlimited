#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Unlimited developers
"""
Tests to check if basic electrum server integration works
"""
import random
from test_framework.util import waitFor, assert_equal
from test_framework.test_framework import BitcoinTestFramework
from test_framework.loginit import logging
from test_framework.electrumutil import compare, bitcoind_electrum_args, ElectrumConnection, create_electrum_connection
from test_framework.nodemessages import FromHex, CTransaction, CTxOut, ToHex
from test_framework.blocktools import create_transaction
from test_framework.cashaddr import decode as decode_addr
from test_framework.script import *

CASHACCONT_PREFIX = bytes.fromhex("01010101")
DATATYPE_KEYHASH = bytes.fromhex("01")

class ElectrumBasicTests(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [bitcoind_electrum_args()]

    def create_cashaccount_tx(self, n, spend, name, address):
        fee = 500
        tx = create_transaction(
            FromHex(CTransaction(), n.getrawtransaction(spend['txid'])),
            spend['vout'], b"", spend['satoshi'] - fee)

        _, _, keyhash = decode_addr(address)

        name = bytes(name, 'ascii')
        keyhash = DATATYPE_KEYHASH + keyhash

        cashaccount_script = CScript([OP_RETURN, CASHACCONT_PREFIX, name, keyhash])

        tx.vout.append(CTxOut(0, cashaccount_script))
        tx.rehash()
        return n.signrawtransaction(ToHex(tx))['hex']

    def sync_electrs(self, n):
        waitFor(10, lambda: compare(n, "index_height", n.getblockcount()))

    def test_basic(self, n, spends):
        # Tests adding one cashaccount registration
        tx = self.create_cashaccount_tx(n, spends.pop(), "satoshi", n.getnewaddress())
        n.sendrawtransaction(tx)

        n.generate(1)
        self.sync_electrs(n)

        res = self.electrum.call("cashaccount.query.name", "satoshi", n.getblockcount())
        assert_equal(1, len(res['result']))
        account = res['result'][0]
        assert_equal(tx, account['tx'])
        assert_equal(n.getblockcount(), account['height'])
        assert_equal(n.getbestblockhash(), account['blockhash'])

    def test_duplicate_registration(self, n, spends):
        # If a block has multiple transactions registering the same name, all
        # should be returned
        tx1 = self.create_cashaccount_tx(n, spends.pop(), "nakamoto", n.getnewaddress())
        tx2 = self.create_cashaccount_tx(n, spends.pop(), "nakamoto", n.getnewaddress())
        # Case of name should be ignored when retrieving accounts
        tx3 = self.create_cashaccount_tx(n, spends.pop(), "NakaMOTO", n.getnewaddress())
        for tx in [tx1, tx2, tx3]:
            n.sendrawtransaction(tx)
        n.generate(1)
        self.sync_electrs(n)

        res = self.electrum.call("cashaccount.query.name",
                "nakamoto",
                n.getblockcount())

        txs = list(map(lambda r: r['tx'], res['result']))
        assert_equal(3, len(txs))
        assert(tx1 in txs)
        assert(tx2 in txs)
        assert(tx3 in txs)

        for account in res['result']:
            assert_equal(n.getblockcount(), account['height'])
            assert_equal(n.getbestblockhash(), account['blockhash'])

    def run_test(self):
        n = self.nodes[0]
        n.generate(200)
        self.electrum = create_electrum_connection()

        spends = n.listunspent()
        self.test_basic(n, spends)
        self.test_duplicate_registration(n, spends)

    def setup_network(self, dummy = None):
        self.nodes = self.setup_nodes()

if __name__ == '__main__':
    ElectrumBasicTests().main()
