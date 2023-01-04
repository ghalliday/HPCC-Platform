https://mermaid.js.org/syntax/flowchart.html

Using a state diagram
```mermaid
stateDiagram-v2
    state GatherInformation {
        GetHostIP
        GetZapReport
    }
    GatherInformation --> CheckExisting
    CheckExisting --> Reproduce: no

```

Using a flowchart:
```mermaid
flowchart

%% initial flow
    new((New Issue))
    existing{Existing jira?}
    feedbackJira>Feedback jira and workaround]
    version1[Check version]
    upgrade>Upgrade major/minor/point]

%% jira creation
    jira[Create a new jira]
    subgraph gather[Gather Initial Information]
        Environment[Host Environment]
        ZapReport[Zap Report]
        Details[Steps to Reproduce]
    end


%% investigation after jira
    type
    reproduce{Reproduce\nLocally?}

    new --> existing
    existing --> feedbackJira
    existing -- no --> version1
    version1 -- old --> upgrade
    version1 --> jira
    jira --> gather
    gather --> type
    type -- OOM --> serverConfig
    type -- crash --> reproduce

    upgrade .-> jira
    
```

Sequence diagrams could be good for working through designs and documenting .  Adapt to document the udp layer.
```mermaid
sequenceDiagram
    participant Sender
    participant Receiver
    participant Permit
    loop Requester
        Sender->>Receiver: Request to Send
    end
    Receiver->>Sender: Acknowledge
    Receiver-->>Permit: Added to Queue
    Permit->>Sender: Permission to send N
    Note right of Permit: Rational thoughts<br/>prevail...
```
