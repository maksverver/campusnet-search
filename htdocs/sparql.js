function result_to_array(result)
{
    var a = new Array();
    bindings = result.getElementsByTagName('binding');
    for(var n = 0; n < bindings.length; ++n)
        if(bindings[n].hasAttribute('name'))
        {
            var name = bindings[n].getAttribute('name');
            var elem = bindings[n].firstChild;
            switch(bindings[n].firstChild.tagName)
            {
            case 'uri':
            case 'bnode':
                if(elem.firstChild)
                    a[name] = elem.firstChild.data;
                break;
            case 'literal':
                // TODO: parse this!
                if(elem.firstChild)
                    a[name] = elem.firstChild.data;
                break;
            case 'unbound':
                a[name] = null;
                break;
            }
        }
    return a;
}

function parse_sparql_results(doc)
{
    var results = doc.getElementsByTagName('results');
    if(results.length != 1)
        return false;
    else
        results = results[0];

    var result = doc.getElementsByTagName('result');
    var r = new Array();
    for(var n = 0; n < result.length; ++n)
    {
        r[n] = result_to_array(result[n]);
    }
    return r;
}

function parse_sparql_header(doc)
{
    // Find head-element
    var head = doc.getElementsByTagName('head');
    if(head.length != 1)
        return false;
    else
        head = head[0];

    // Check for error
    var error = head.getElementsByTagName('error');
    if(error.length > 0)
        return error[0].firstChild.data;

    // Parse variable headers
    var r = new Array();
    vars = head.getElementsByTagName('variable');
    for(var n = 0; n < vars.length; ++n)
        if(vars[n].hasAttribute('name'))
            r[n] = vars[n].getAttribute('name');
    return r;
}

function parse_sparql(doc)
{
    if(doc == null || doc.tagName != 'sparql')
        return null;

    sparql = Array();

    var header = parse_sparql_header(doc);
    if(typeof header == 'string')
    {
        sparql['error'] = header;
    }
    else
    {
        sparql['header'] = header;
        sparql['results'] = parse_sparql_results(doc);
    }
    return sparql;
}
