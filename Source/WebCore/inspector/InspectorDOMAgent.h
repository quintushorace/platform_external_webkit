/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorDOMAgent_h
#define InspectorDOMAgent_h

#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorValues.h"
#include "Timer.h"

#include <wtf/Deque.h>
#include <wtf/ListHashSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {
class ContainerNode;
class CharacterData;
class Document;
class Element;
class Event;
class InspectorDOMAgent;
class InspectorFrontend;
class MatchJob;
class NameNodeMap;
class Node;
class Page;

#if ENABLE(INSPECTOR)

struct EventListenerInfo {
    EventListenerInfo(Node* node, const AtomicString& eventType, const EventListenerVector& eventListenerVector)
        : node(node)
        , eventType(eventType)
        , eventListenerVector(eventListenerVector)
    {
    }

    Node* node;
    const AtomicString eventType;
    const EventListenerVector eventListenerVector;
};

class InspectorDOMAgent {
public:
    struct DOMListener {
        virtual ~DOMListener()
        {
        }
        virtual void didRemoveDocument(Document*) = 0;
        virtual void didRemoveDOMNode(Node*) = 0;
        virtual void didModifyDOMAttr(Element*) = 0;
    };

    static PassOwnPtr<InspectorDOMAgent> create(InjectedScriptHost* injectedScriptHost, InspectorFrontend* frontend)
    {
        return adoptPtr(new InspectorDOMAgent(injectedScriptHost, frontend));
    }

    InspectorDOMAgent(InjectedScriptHost*, InspectorFrontend*);
    ~InspectorDOMAgent();

    Vector<Document*> documents();
    void reset();

    // Methods called from the frontend for DOM nodes inspection.
    void getChildNodes(long nodeId);
    void setAttribute(long elementId, const String& name, const String& value, bool* success);
    void removeAttribute(long elementId, const String& name, bool* success);
    void removeNode(long nodeId, long* outNodeId);
    void changeTagName(long nodeId, const String& tagName, long* newId);
    void getOuterHTML(long nodeId, WTF::String* outerHTML);
    void setOuterHTML(long nodeId, const String& outerHTML, long* newId);
    void setTextNodeValue(long nodeId, const String& value, bool* success);
    void getEventListenersForNode(long nodeId, long* outNodeId, RefPtr<InspectorArray>* listenersArray);
    void addInspectedNode(long nodeId);
    void performSearch(const String& whitespaceTrimmedQuery, bool runSynchronously);
    void searchCanceled();
    void resolveNode(long nodeId, RefPtr<InspectorValue>* result);
    void getNodeProperties(long nodeId, PassRefPtr<InspectorArray> propertiesArray, RefPtr<InspectorValue>* result);
    void getNodePrototypes(long nodeId, RefPtr<InspectorValue>* result);
    void pushNodeToFrontend(PassRefPtr<InspectorObject> objectId, RefPtr<InspectorValue>* result);

    // Methods called from the InspectorInstrumentation.
    void setDocument(Document*);
    void releaseDanglingNodes();

    void mainFrameDOMContentLoaded();
    void loadEventFired(Document*);

    void didInsertDOMNode(Node*);
    void didRemoveDOMNode(Node*);
    void didModifyDOMAttr(Element*);
    void characterDataModified(CharacterData*);

    Node* nodeForId(long nodeId);
    long pushNodePathToFrontend(Node*);
    void pushChildNodesToFrontend(long nodeId);
    void pushNodeByPathToFrontend(const String& path, long* nodeId);
    long inspectedNode(unsigned long num);
    void copyNode(long nodeId);
    void setDOMListener(DOMListener*);

    String documentURLString(Document*) const;

    // We represent embedded doms as a part of the same hierarchy. Hence we treat children of frame owners differently.
    // We also skip whitespace text nodes conditionally. Following methods encapsulate these specifics.
    static Node* innerFirstChild(Node*);
    static Node* innerNextSibling(Node*);
    static Node* innerPreviousSibling(Node*);
    static unsigned innerChildNodeCount(Node*);
    static Node* innerParentNode(Node*);
    static bool isWhitespace(Node*);

private:
    // Node-related methods.
    typedef HashMap<RefPtr<Node>, long> NodeToIdMap;
    long bind(Node*, NodeToIdMap*);
    void unbind(Node*, NodeToIdMap*);

    bool pushDocumentToFrontend();

    bool hasBreakpoint(Node*, long type);
    void updateSubtreeBreakpoints(Node* root, uint32_t rootMask, bool value);
    void descriptionForDOMEvent(Node* target, long breakpointType, bool insertion, PassRefPtr<InspectorObject> description);

    PassRefPtr<InspectorObject> buildObjectForNode(Node*, int depth, NodeToIdMap*);
    PassRefPtr<InspectorArray> buildArrayForElementAttributes(Element*);
    PassRefPtr<InspectorArray> buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap);
    PassRefPtr<InspectorObject> buildObjectForEventListener(const RegisteredEventListener&, const AtomicString& eventType, Node*);

    void onMatchJobsTimer(Timer<InspectorDOMAgent>*);
    void reportNodesAsSearchResults(ListHashSet<Node*>& resultCollector);

    Node* nodeForPath(const String& path);
    PassRefPtr<InspectorArray> toArray(const Vector<String>& data);

    void discardBindings();

    InjectedScript injectedScriptForNodeId(long nodeId);

    InjectedScriptHost* m_injectedScriptHost;
    InspectorFrontend* m_frontend;
    DOMListener* m_domListener;
    NodeToIdMap m_documentNodeToIdMap;
    // Owns node mappings for dangling nodes.
    Vector<NodeToIdMap*> m_danglingNodeToIdMaps;
    HashMap<long, Node*> m_idToNode;
    HashMap<long, NodeToIdMap*> m_idToNodesMap;
    HashSet<long> m_childrenRequested;
    long m_lastNodeId;
    RefPtr<Document> m_document;
    Deque<MatchJob*> m_pendingMatchJobs;
    Timer<InspectorDOMAgent> m_matchJobsTimer;
    HashSet<RefPtr<Node> > m_searchResults;
    Vector<long> m_inspectedNodes;
};

#endif // ENABLE(INSPECTOR)

} // namespace WebCore

#endif // !defined(InspectorDOMAgent_h)