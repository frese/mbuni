// Utillity javascript functions, some taken from Apple Computer, Inc. sample code.
// modified to suite our purposses.
//
// (c) Allan Frese, Realtime A/S


function getStyleObject(objectId) {
    // cross-browser function to get an object's style object given its id
    if(document.getElementById && document.getElementById(objectId)) {
	// W3C DOM
	return document.getElementById(objectId).style;
    } else if (document.all && document.all(objectId)) {
	// MSIE 4 DOM
	return document.all(objectId).style;
    } else if (document.layers && document.layers[objectId]) {
	// NN 4 DOM.. note: this won't find nested layers
	return document.layers[objectId];
    } else {
	return false;
    }
} // getStyleObject

function changeObjectVisibility(objectId, newVisibility) {
    // get a reference to the cross-browser style object and make sure the object exists
    var styleObject = getStyleObject(objectId);
    if(styleObject) {
	styleObject.visibility = newVisibility;
	return true;
    } else {
	// we couldn't find the object, so we can't change its visibility
	return false;
    }
} // changeObjectVisibility

function moveObject(objectId, newXCoordinate, newYCoordinate) {
    // get a reference to the cross-browser style object and make sure the object exists
    var styleObject = getStyleObject(objectId);
    if(styleObject) {
	styleObject.left = newXCoordinate;
	styleObject.top = newYCoordinate;
	return true;
    } else {
	// we couldn't find the object, so we can't very well move it
	return false;
    }
} // moveObject

function DivOn(divObj)
{
    var entries, entry;

    if (document.getElementsByTagName && document.getElementsByTagName("DIV")) {
	entries = document.getElementsByTagName("DIV");
	for (i=0; i<entries.length; i++) {
	    entry = entries[i];
	    if (entry.id == divObj) {
		entry.style.display = "block";
	    }
	}
    } else {
	entries = document.all.tags("DIV");
	for (i=0; i<entries.length; i++) {
	    entry = entries[i];
	    if (entry.id == divObj) {
		entry.style.display = "block";
	    }
	}
    }
}

function DivOff(divObj)
{
    var entries, entry;

    if (document.getElementsByTagName && document.getElementsByTagName("DIV")) {
	entries = document.getElementsByTagName("DIV");
	for (i=0; i<entries.length; i++) {
	    entry = entries[i];
	    if (entry.id == divObj) {
		entry.style.display = "none";
	    }
	}
    } else {
	entries = document.all.tags("DIV");
	for (i=0; i<entries.length; i++) {
	    entry = entries[i];
	    if (entry.id == divObj) {
		entry.style.display = "none";
	    }
	}
    }
}

// Set focus on login box, if it exists
function setFocus() {
    if (document.login && document.login.digest) {
	document.login.digest.focus();
    }
}


function confirmit(question)
{
    if (confirm(question)) {
	return true;
    } else {
	return false;
    }
}
