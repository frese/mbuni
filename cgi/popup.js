// Popup help bubbles, mostly taken from Apple Computer, Inc. sample code
// modified to suite our purposses.
//
// (c) Allan Frese, Realtime A/S

// store variables to control where the popup will appear relative to the cursor position
// positive numbers are below and to the right of the cursor, negative numbers are above and to the left
var xoffset = -20;
var yoffset = -110;

function showPopup (targetObjectId, eventObj) {

    if(eventObj) {
	// hide any currently-visible popups
	hideCurrentPopup();

	// stop event from bubbling up any farther
	eventObj.cancelBubble = true;

	// move popup div to current cursor position 
	var newXCoordinate = (eventObj.pageX) ? eventObj.pageX + xoffset : eventObj.clientX + xoffset + ((document.body.scrollLeft)?document.body.scrollLeft:0);
	var newYCoordinate = (eventObj.pageY) ? eventObj.pageY + yoffset : eventObj.clientY + yoffset + ((document.body.scrollTop)?document.body.scrollTop:0);

	if (newXCoordinate > 600) {
	    newXCoordinate = 600;
	}
	moveObject(targetObjectId, newXCoordinate, newYCoordinate);

	// and make it visible
	if( changeObjectVisibility(targetObjectId, 'visible') ) {
	    // if we successfully showed the popup
	    // store its Id on a globally-accessible object
	    window.currentlyVisiblePopup = targetObjectId;
	    return true;
	} else {
	    // we couldn't show the popup, boo hoo!
	    return false;
	}
    } else {
	// there was no event object, so we won't be able to position anything, so give up
	return false;
    }
} // showPopup

function hideCurrentPopup() 
{
    // note: we've stored the currently-visible popup on the global object window.currentlyVisiblePopup
    if(window.currentlyVisiblePopup) {
	changeObjectVisibility(window.currentlyVisiblePopup, 'hidden');
	window.currentlyVisiblePopup = false;
    }
} // hideCurrentPopup


// ***********************
// hacks and workarounds *
// ***********************

// initialize hacks whenever the page loads
window.onload = initializeHacks;

// setup an event handler to hide popups for generic clicks on the document
document.onclick = hideCurrentPopup;

function initializeHacks() {
    // this ugly little hack resizes a blank div to make sure you can click
    // anywhere in the window for Mac MSIE 5
    if ((navigator.appVersion.indexOf('MSIE 5') != -1) 
	&& (navigator.platform.indexOf('Mac') != -1)
	&& getStyleObject('blankDiv')) {
	window.onresize = explorerMacResizeFix;
    }
    resizeBlankDiv();
    // this next function creates a placeholder object for older browsers
    createFakeEventObj();
}

function createFakeEventObj() {
    // create a fake event object for older browsers to avoid errors in function call
    // when we need to pass the event object to functions
    if (!window.event) {
	window.event = false;
    }
} // createFakeEventObj

function resizeBlankDiv() {
    // resize blank placeholder div so IE 5 on mac will get all clicks in window
    if ((navigator.appVersion.indexOf('MSIE 5') != -1) 
	&& (navigator.platform.indexOf('Mac') != -1)
	&& getStyleObject('blankDiv')) {
	getStyleObject('blankDiv').width = document.body.clientWidth - 20;
	getStyleObject('blankDiv').height = document.body.clientHeight - 20;
    }
}

function explorerMacResizeFix () {
    location.reload(false);
}

