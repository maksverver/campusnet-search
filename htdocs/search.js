function parse_terms(str)
{
    terms = str.split(/ +/);
    result = Array();
    for(n in terms)
        if(terms[n] != '')
            result[result.length] = terms[n];
    return result;
}

function search(terms, button, results_elem)
{
    terms = parse_terms(terms);
    if(terms.length == 0)
    {
        alert('Query was empty!');
        return false;
    }

    terms_pattern = '?uri :keyword ';
    for(n = 0; n < terms.length; ++n)
    {
        terms_pattern += '"' + terms[n].replace(/"/, '\\"') + '"';
        terms_pattern += (n == terms.length - 1) ? ' . ' : ' , ';
    }

    query = 'PREFIX : <http://maks.student.utwente.nl/ns/smb#>\n' +
            'SELECT ?uri ?type ?size ?files ?digest ?mediaType ?error\n' +
            'WHERE { ' + terms_pattern +
                     '?uri :type ?type.\n' +
                     'OPTIONAL { ?uri :size      ?size} .\n' +
                     'OPTIONAL { ?uri :files     ?files} .\n' +
                     'OPTIONAL { ?uri :digest    ?digest } .\n' +
                     'OPTIONAL { ?uri :mediaType ?mediaType } .\n' +
                     'OPTIONAL { ?uri :error     ?error} }\n' +
            'LIMIT 500';

    function callback(request)
    {
        button.disabled = false;
        var result;

        if( request.responseXML == null ||
            (result = parse_sparql_results(request.responseXML.documentElement)) == null)
        {
            alert( 'Failed to parse SPARQL results!\n\n' +
                   'Query was:\n----------\n' + query + '\n\n' +
                   'Response was:\n---------\n' + request.responseText);
        }
        else
        if(typeof result == 'string')
        {
            // Display error message
            alert( 'Unable to execute SPARQL query!\n\n' +
                   'Query was:\n----------\n' + query + '\n\n' +
                   'Error was:\n---------\n' + result );
        }
        else
        {
            // Display results
            display_results(result, results_elem);
        }
    }

    // Perform request
    button.disabled = true;
    request('webquery.fcgi', callback, 'POST', query, 'application/sparql-query');
    return true;
}

function display_results(results, elem)
{
    while(elem.firstChild)
        elem.removeChild(elem.lastChild);

    if(results.length == 0)
    {
        span = document.createElement('span');
        span.className = 'noresults';
        span.appendChild(document.createTextNode('No results found!'));
        elem.appendChild(span);
    }
    else
    {
        for(n in results)
            elem.appendChild(construct_result(results[n]));
    }
}

function construct_result(result)
{
    elem = document.createElement('span');
    elem.className = 'result';
    elem.appendChild(document.createTextNode(unescape(result['uri'])));
    return elem;
}
