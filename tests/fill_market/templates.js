
const exportAsset = (a) => {
    return `
_db.create<asset_object>([&](auto& a) {
    a.creator = "${a.creator}";
    a.max_supply = asset::from_string("${a.max_supply}");
    a.supply = a.max_supply;
    a.supply.amount = 0;
    a.allow_fee = ${a.allow_fee};
    a.allow_override_transfer = ${a.allow_override_transfer};
    if (_db.store_asset_metadata()) {
        std::string jm = ${JSON.stringify(a.json_metadata)};
        from_string(a.json_metadata, jm);
    }
    a.created = _db.head_block_time();
    a.fee_percent = ${a.fee_percent};
});
`
}

const reversePrice = (price) => {
    const base = price.quote
    const quote = price.base
    return { base, quote }
}

const exportOrder = (o) => {
    const { sym1, sym2 } = o
    let price = JSON.parse(o.order_price)
    let { bid, asset1, asset2 } = o

    let reversed = false
    if (sym2 === 'GOLOS' || (sym2 < sym1 && sym1 !== 'GOLOS')) {
        reversed = true
    }

    if (reversed) {
        bid = !bid;
        price = reversePrice(price);
        [asset1, asset2] = [asset2, asset1];
    }

    return `
// is_bid: ${bid}
_db.create<limit_order_object>([&](limit_order_object &obj) {
    obj.created = _db.head_block_time();
    obj.seller = "${o.seller}";
    obj.orderid = ${o.orderid};
    obj.for_sale = ${bid ? asset2 : asset1};
    obj.symbol = ${bid ? `asset::from_string("${price.base}").symbol` : `asset::from_string("${price.base}").symbol`};
    obj.sell_price = price(asset::from_string("${price.base}"), asset::from_string("${price.quote}"));
    obj.expiration = time_point_sec::maximum();
});
`
}

module.exports = {
    exportAsset,
    exportOrder,
}
