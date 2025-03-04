const fs = require('fs')

const arg = require('arg')
const golos = require('golos-lib-js')
const { Asset } = require('golos-lib-js/lib/utils')

const Database = require('./db.js')
const { exportAsset, exportOrder }= require('./templates.js')

const main = async () => {
    const args = arg({
        '--node': String,

        '-n': '--node',
    })

    let syms = args._[0]
    if (!syms) throw new Error('No syms arg')
    syms = JSON.parse(syms)
    if (!Array.isArray(syms) || syms.length < 2 ) throw new Error('Wrong syms arg')

    const node = args['--node'] || 'wss://apibeta.golos.today/ws'
    console.log('Source node:', node)
    golos.config.set('websocket', node)

    const db = await Database.build()

    const allSyms = new Set()

    const saveSyms = async (ord) => {
        const { order_price } = ord
        const base = await Asset(order_price.base)
        const quote = Asset(order_price.quote)
        allSyms.add(base.symbol)
        allSyms.add(quote.symbol)
    }

    for (let i = 0; i < syms.length - 1; ++i) {
        const sym1 = syms[i]
        const sym2 = syms[i+1]
        const pair = [sym1, sym2]
        console.log('-- Processing pair:', pair)
        const ords = await golos.api.getOrderBookExtendedAsync(1000, pair)
        if (ords.bids[0]) await saveSyms(ords.bids[0])
        for (const ord of ords.bids) {
            await db.saveOrder(sym1, sym2, true, ord)
        }
        console.log('-- Bids done, saving asks...')
        for (const ord of ords.asks) {
            await db.saveOrder(sym1, sym2, false, ord)
        }
        console.log('-- Done')
    }

    console.log('-- Processing assets...')
    allSyms.delete('GOLOS')
    allSyms.delete('GBG')

    const assets = await golos.api.getAssetsAsync('', [...allSyms])
    for (const a of assets) {
        const sym = a.supply.split(' ')[1]
        console.log('-- Saving asset', sym)
        await db.saveAsset(sym, a)
    }

    console.log('-- Done assets.') 

    console.log('-- Generating code (Assets).') 

    let code = ''

    const oAssets = await db.getAssets()
    for (const a of oAssets) {
        code += exportAsset(a.dataValues)
    }

    code += "\nelog(\"* ------------------- LIMIT ORDERS \");\n\n"

    console.log('-- Generating code (Orders).') 

    const oOrders = await db.getOrders()
    for (const o of oOrders) {
        code += exportOrder(o.dataValues)
    }

    console.log('-- Done. Saving code..')

    fs.writeFileSync('code.txt', code)

    const hf_actions = '../../libraries/chain/hf_actions.cpp'
    const start='/* FILL_DATA */'
    const end='/* FILL_DATA_END */'

    const hfa = fs.readFileSync(hf_actions,'utf-8')
    let res = hfa.split(start)[0]
    res += start + code
    res += end + hfa.split(end)[1]

    fs.writeFileSync(hf_actions, res)

    console.log('-- Done!')

    process.exit()
}

main()
