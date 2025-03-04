const { Sequelize, DataTypes } = require('sequelize')

class Database {
    constructor() {
        this.seq = new Sequelize({
           dialect: 'sqlite', storage: 'orders.sqlite'
        })
    }

    static build = async () => {
        const db = new Database()
        await db.defineModels()
        return db
    }

    defineModels = async () => {
        this.Order = this.seq.define(
            'Order',
            {
                sym1: { type: DataTypes.STRING, },
                sym2: { type: DataTypes.STRING, },
                bid: { type: DataTypes.BOOLEAN, },

                orderid: { type: DataTypes.STRING, },
                order_price: { type: DataTypes.STRING, },
                real_price: { type: DataTypes.STRING, },
                asset1: { type: DataTypes.STRING, },
                asset2: { type: DataTypes.STRING, },
                created: { type: DataTypes.STRING, },
                seller: { type: DataTypes.STRING, },
            },
            {
            },
        )

        this.Asset = this.seq.define(
            'Asset',
            {
                sym: { type: DataTypes.STRING, },

                creator: { type: DataTypes.STRING, },
                max_supply: { type: DataTypes.STRING, },
                supply: { type: DataTypes.STRING, },
                can_issue: { type: DataTypes.STRING, },
                precision: { type: DataTypes.INTEGER, },
                allow_fee: { type: DataTypes.BOOLEAN, },
                allow_override_transfer: { type: DataTypes.BOOLEAN, },
                json_metadata: { type: DataTypes.STRING, },
                created: { type: DataTypes.STRING, },
                modified: { type: DataTypes.STRING, },
                marketed: { type: DataTypes.STRING, },
                symbols_whitelist: { type: DataTypes.STRING, },
                fee_percent: { type: DataTypes.INTEGER, },
            },
            {
            },
        )

        await this.Order.sync({ force: true })
        await this.Asset.sync({ force: true })
    }

    saveOrder = async (sym1, sym2, bid, ord) => {
        const orderid = ord.orderid.toString()
        const order_price = JSON.stringify(ord.order_price)
        const real_price = ord.real_price.toString()
        const asset1 = ord.asset1.toString()
        const asset2 = ord.asset2.toString()
        const created = ord.created.toString()
        const seller = ord.seller.toString()
        await this.Order.create({
            sym1, sym2, bid,
            orderid, order_price, real_price, asset1, asset2, created, seller,
        }, {
            logging: false,
        })
    }

    saveAsset = async (sym, a) => {
        const creator = a.creator.toString()
        const max_supply = a.max_supply.toString()
        const supply = a.supply.toString()
        const can_issue = a.can_issue.toString()
        const precision = a.precision
        const allow_fee = a.allow_fee
        const allow_override_transfer = a.allow_override_transfer
        const json_metadata = a.json_metadata.toString()
        const created = a.created.toString()
        const modified = a.modified.toString()
        const marketed = a.marketed.toString()
        const symbols_whitelist = JSON.stringify(a.symbols_whitelist)
        const fee_percent = a.fee_percent
        await this.Asset.create({
            sym, creator, max_supply, supply, can_issue, precision,
            allow_fee, allow_override_transfer, json_metadata, created, modified,
            marketed, symbols_whitelist, fee_percent,
        }, {
            logging: false,
        })
    }

    getAssets = async () => {
        return await this.Asset.findAll()
    }

    getOrders = async () => {
        return await this.Order.findAll()
    }
}

module.exports = Database
