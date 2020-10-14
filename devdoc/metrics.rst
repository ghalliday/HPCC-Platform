HPCC System Metrics
===================

This document examines gather metrics, particularly system metrics, and publishing them to an external service for aggregation and analysis.  Metrics are useful for the following reasons:

* Alerts and Health monitoring
* Auto scaling
* Fault diagnosis and resource monitoring
* Analysis of jobs/workunits and profiling

1. Alerts and health monitoring.
The metrics used for alerts should be tied to symptoms that the user will see (e.g. query time), or that strongly indicate problems will follow (e.g. low disk space).  Note, alerts will be raised by the external service, from the aggregated results from all the components, not by the HPCC system itself.

2. Auto scaling
The metrics used for autoscaling need to be strongly correlated with whether a resource should be scaled up or down.  Again the values used for the scaling will be aggregated from the metrics published by multiple replicas running on different pods.
To support the Kubernetes Horizontal Pod Autoscaler (HPA) the value of these metrics should be proportional to the number of pods that are required.  For example, scaling on the number of items waiting on a queue does not really give you what you want.  If all your replicas are busy processing items, and there are no items on the queue, that does not mean you can scale down to 0 replicas!  In this case the scaling metric should include the number of items being actively processed as well, i.e. demand = waiting + inflight.
It must also be possible to directly map 1:1 from a metric to the component that can be scaled.  See eclccserver discussion below for an example of this requirement.

NOTE: Currently the approach for scaling is for the the qwait component to be allowed to run an arbitrary number of jobs - so the number of child instances (processes or k8s jobs) will match the number of requests.  The number of stub replicas is used to provide resilience.  This approach has a few restrictions
- It is very hard to place an upper limit on the number of jobs.
- It does not map very well to using child processes.  (Possibly you could use the vertical Pod Autoscaler.)
- If operations are short compared with the time to start of k8s job the operator may want to configure some components to linger and process other items.  I don't know which of these makes it easier or harder.  I suspect K8s autoscaling would ramp up and down more smoothly.

If the current approach is retained then there is little point in publishing the metrics that could be used for autoscaling (unless they are useful for another reason).

3. Fault diagnosis and resource monitoring
They are useful for gaining an overall idea of the health of a cluster, but are particularly important for diagnosing what is going wrong when a problem has occurred - e.g. cpu usage, latency accessing LDAP.

4. Analysis of workunits
Either to improve performance or fault diagnosis.  These are already gathered in the workunit statistics.  It should also be generate alerts for a workunit - e.g., if the expected time for a compile or running a job is exceeded - and guillotine the job if necessary.
Should these also be exported to the external service?  I think not for a couple of reasons.  Firstly they are connected with the characteristics of a particular job, rather than the health of the system as a whole.  The quantity of metrics is also likely to swamp any external server.  (We do not currently support time-series storage for these stats due to the size.)  They are very useful when delving deeper into a problem though.

5. Events
It **would** be very useful if key events - when executing a workunit, or spraying a file etc - were published to an external service (e.g. datadog) so they could be correlated with the metric information.  It would help tie down the cause of issues, and point at workunits which may have contributed to the problem.  (These could probably be automatically exported from the WhenXX stats already stored.)  That is a separate question outside the scope of this document.

A note on percentiles/quantiles
-------------------------------
Naively to monitor the health of a service that requires a 95 percentile response of say 200ms, you would expect the component to publish a 95 percentile time, and for an alert to be raised if it gets close to the 200ms threshold.  However there are several problems with that approach.
- calculating the 95%tile is hard.
  For instance, you could store all the values in order, and retrive the n-tile from the sorted list, but that is likely to be computationally expensive.  There are methods for approximating the result (using a sliding window), but they are likely to be complex.
- the results do not aggregate over replicas
  If you have 95%tile results from 5 replicas it isn't possible to work out the 95%tile for all the replicas.  You would need to have a single element/component collating all the results and extract it from that.  That will prohibitively add to the complexity and cost and even more if you also publish information about the individual replicas (to be able to spot problems with those.)
- the results do not aggregate over time
  If the times are a similar duration to the metric collection frequency there will not be enough data points to calculate the 95%tile, and results from multiple results cannot be combined to workaround the lack of data.

An alternative to publishing quantiles is to publish a histogram which contain counts of metrics less than a set of thresholds.  So in the example above you might count responses < a 200ms threshold and total counts.  If the counts above the 200 threshold exceed 5% of the total counts then an alert can be raised.  In practice you will want to gather counts for other thresholds e.g. <150ms, <175ms to give an idea of the spread in query times.  This solves the problems with reporting quantiles, but requires the thresholds to be supplied to the code that is calculating the metrics.  For at least some (probably all) of the histogram metrics those thresholds will need to be configurable.

We should clarify where these different thresholds are specified.  Thresholds for components probably need to be configured in the helm files.  Thresholds for roxie services should be in the work unit, or when the query is deployed.  Should thresholds for esp should be in the ESDL definition?

https://prometheus.io/docs/practices/histograms/ contains a helpful discussion.

Note the counts within the histogram are cumulative over time - it is the responsibility of the sink to use differences to calculate rates.

Time metrics
------------
What happens about time periods that are longer than the collecting interval?  Examples would be the time eclagent spends waiting for esp when spraying a file, or eclccserver spends compiling a complex roxie query.  It is important that metrics for long running operations are updated while they are executing, rather than at the end.  The histogram buckets are cumulative, which means the metric can be cleanly reported - provided the bucket for the time spent so far is correctly incremented.  It will need an (alternative) implementation for the timing that correctly reports the time elapsed if it has not completed.

If the time is likely to be the same order/larger than the reporting period then there is little benefit in reporting a histogram of times
- it is likely to be hard to pick the thresholds
- a simple counter.time. and counter.count for the metric is likely to be sufficient.

If the metric is used for resource utilization rather than alerting then a count/sum will also be sufficient.

If an external resource is globally publishing work related metrics (e.g. a histogram of time taken), then I think it is sufficient for a component to record the total time spent waiting for that resource/service.  If it is an external component that is not generating metrics and an alert is required then the histogram should be published by the component (e.g. LDAP access from esp).

Component lifetime
------------------
It is assumed that the lifetime of the components that are publishing the metrics are generally much longer than the metric reporting period.  It will generally be the components waiting for items on a queue that report the metrics, rather than the child processes which are then started.

Metrics will always be approximate.  There will be situations where pods go down before the metrics are collected, and multiple replicas of components exist - e.g. when updating an installation even if not autoscaling.  The metric server is responsible for cleaning up the data.

Metric details
--------------

The metrics that are collected have the following characteristics:
 a. a kind e.g. a counter (always increases), gauge (goes up and down)
 b. The type of a unit, each of which has an associated uint.  count [], time [seconds], size [bytes]
 c. any aggregation done on the value - e.g. average, max, histogram
 d. the quantity that is being measured e.g. requests, memory
 e. A characteristic of the quantity e.g. success, failure, timeout
 f. Which element (often within the component) the metric relates to e.g.  ws_workunits, thor400.eclccserver.queue
 g. which component is generating the metric - e.g. thor, eclccserver, dali
 h. the name of the component instance - e.g. thor400, myeclccserver

(g and h) are fixed for all metrics in a running component.

The discussion below uses the following format as a suggestion for how the metric is identified:

    <kind>.type[.<aggregation>].<quantity>[:<characteristic>]/<element>
e.g.
    counter.count.histogram.request/dali
    counter.time.max.request:timeout/thor400.eclccserver.queue

{ This to help describe the metrics, rather than a proposal for implementation.  I would expect any implementation to pass many of these fields separately (and use enumerated types) to make it easy to extract the relevant elements.}

A suggested mapping to the following prometheus label convention:

    hpcc_<element>_<quantity>_<unit>_<aggregation>, with the label being used for the characteristic of the quantity.

e.g.

    hpcc_dali_request_seconds_histogram
    hpcc_thor400_eclccserver_queue_request_seconds_max{label="timeout",instance="myeclccserver"}

side note: For the workunit statistics, the stats names have the form <unit><aggregation><quantity+characteristic>.  The scope provides the element.  The characteristic is combined with the quantity.  Only count kinds are really supported.

Components
==========
The following is a list of the different HPCC components and the metrics that we aim to collect.  The specification of the metric is a first iteration - I suspect some of them are not correct:  

Dali
----
Dali does not support multiple replicas, so there are no metrics specifically aimed at supporting autoscaling.  If some of the data in dali (e.g. workunits/meta data) is moved to a different location (e.g. cassandra) it may be possible to scale that resource.

Metrics:
* Time to process a request [counter.count.histogram.request:process/dali, gauge.time.max.request/dali, counter.count.request/dali, counter.time.request/dali ].
  Average can be calculated from time and count.  For simplicity, this combined set of stats will be referred to as {time-histogram}.request/dali in subsequent references.
  Should a gauge be used for the maximum time since the last request?
* Number of requests processed [counter.count.request/dali]
* Number of requests successful [counter.count.request:success/dali]
* Number of request timeouts [counter.count.request:timeout/dali]
* Number of request failures [counter.count.request:failure/dali]
Wait time for a request [{time-histogram}.request:wait/dali]?

What other metrics would be useful for diagnosis/health, and cheap to gather?
- Size of external data read/written (counter.size.external_read)? 
- Size of the total store?
- Lock stats?  [ gauge.count.locks/dali, gauge.count.locks:timeout/dali, gauge.count.pending-locks/dali ]

NB: It is only worth gathering and reporting metrics if it is likely to be useful for alerting, scaling or diagnosis.  Only report it if you have a concrete idea of how it can be used.

eclccserver
-----------
The number of eclccserver instances can be scaled to match the demand.  However, the time taken to compile a query can vary very widely, and in many cases will exceed the time between reporting metrics.

Possible scaling metrics:
Number of items on the queue [ gauge.count.request/<queue>.eclccserver.queue ]
Number of items inflight [ gauge.count.request:processing/<queue>.eclccserver.queue ]
?Demand (on queue + inflight)?  [ gauge.count.demand/<queue>.eclccserver.queue ]

{NOTE: Is using a gauge for these the correct approach?  Is the best solution to use counters and for the metric server to calculate a rate and publish that as a derived metric? Leaving as a gauge for the moment since that reflects the intent of the autoscaling metric.}

This latter "demand" metric is most likely to be proportional to the number of eclccservers needed to manage demand (see the discussion in the introduction).  The same scaling logic applies whether there is a 1:1 correspondence between the qwait daemons, or 1:N.

There are several questions about how that scaling can be achieved in practice:
- How are the metrics reported?
  
  The number of items on the queue is something that needs to be reported by a single component, rather than each of the eclccserver instances, since it is a single global quantity.  This will possibly require a new light-weight component that reports queue stats.  Alternatively it could be included into an existing single instance component (e.g. dali/sasha).  Wherever it lives it is likely to be responsible for reporting similar metrics for all dali queues.

  The number inflight would most naturally be reported from the eclccserver replicas - publishing the demand should also be possible from the replicas, but are there any costs or implications of it getting the number of active queue items?  A better approach is likely to be aggregating requests and inflight from all replicas in the metric server, and publish a new derived metric.

Currently one eclccserver instance may process queries for multiple queues.  Jake raised the question, why do we have this special rule for eclccservers?  Should each component roxie/eclagent/thor have its own dedicated eclccserver instead?  It would simplify the auto scaling, and probably simplify the helm scripts.  Assuming it stays as it is...

- How is the demand for a queue related to the scaling for eclccservers (since an eclccserver listens to multiple queues)?  I can think of a few options:
  - a single replicated instance of eclccserver.
    In this case it would scale on [ gauge.count.demand/*.eclccserver.queue ] - and the metric published would be the sum for all active queues.
    (Can this be done with our proposed prometheus mapping?)
  - one instance tied to a particular queue, and another that handles all other queues.
    The first instance would be scaled on [ gauge.count.demand/<special-queue>.eclccserver.queue ], the second on [ gauge.count.demand/*.eclccserver.queue - gauge.count.demand/<special-queue>.eclccserver.queue ].  This knowledge would need to be embedded in the system somewhere - probably in the metrics server that aggregates the results.  This case would also extend to multiple eclccservers tied to a single queue, and others processing the rest.
  - Multiple eclccserver instances processing multiple queues.  I can't off the top of my head see how this can auto scale unless the queues are disjoint.


Alert/Diagnosis metrics:

Time to process a request [{time-histogram}.request/<eclccserver-name>.eclccserver].
Number of requests processed/successes [counter.count.request[:success]/<eclccserver-name>.eclccserver]
Number of request failures [counter.count.request:failure/<eclccserver-name>.eclccserver]
Wait time for a request?  [{time-histogram}.request:wait/<queue>.eclccserver.queue]  Is this useful, or just noise?  I am also not sure if prometheus would recommend having this as a different metric, or having label differentiate it from processed/successful metrics.

I think it may be necessary to publish a request:wait metric and a request:processing metric and calculate the total time from that.  Whatever solution is used the waiting/total time needs to increase while the items are still on the queue.

I suspect any component that listens to a queue will want a very similar set of metrics - with extra metrics for each of the different notable causes of failure e.g. abort,failure,timeout.

qwait/eclagent
--------------

Scaling metrics:

Scaling is likely to be very similar to eclccserver, but monitoring the execution queue, rather than the compile queue:

Number of items on the queue [ gauge.count.request/<queue>.eclagent.queue ]
Number of items inflight [ gauge.count.inflight/<queue>.eclagent.queue ]
?Demand (on queue + inflight)?  [ gauge.count.demand/<queue>.eclagent.queue ]

There is a 1:1 correspondence between the queue names and the instances of eclagent, so none of complications that eclccserver currently has.

Alert/Diagnosis metrics:

In addition to the standard metrics for monitoring something waiting on a queue, it is likely to want to publish stats about the time taken to start k8s jobs.  E.g.

Wait time to start a child k8s job:  [{time-histogram}.wait/k8s.<queue>.eclagent]

This is another case where the time period is likely to be similar to or possibly longer than the metric reporting period.  It would be simpler and possibly just as useful to publish the time spent waiting, rather than the histogram (see discussion above).

For eclagent the alert/diagnosis metrics would also benefit from time spent accessing important resources:

Wait time requesting information from dali:  [ counter.time.dali_request:wait/<queue>.eclagent ] is probably sufficient, but may require [{time-histogram}.wait.dali.<queue>.eclagent].  I suspect the total time may be sufficient to spot a problem, and then details of response times from dali can be used to investigate the cause.

Wait time requesting information from esp:  [ counter.time.esp_request:wait/esp.<queue>.eclagent ] (or [{time-histogram}.esp_request:wait/esp.<queue>.eclagent])

{eclagent is still a confusing term, can we revisit it and see if we can improve it }

Thor
----

Very similar to eclagent - it similarly needs to know times for dali, esp, and k8s for the qwait part of the component.  

There may be other custom metrics within a query that are useful to publish e.g. time spent in the address cleaner.  If so that raises the question of how they would be published from a "short-lived" component (see questions at the end).  Also,

Number of request failures [counter.count.request:failure/<thor-name>.thor]
Number of request aborts [counter.count.request:abort/<thor-name>.thor]

{I'm not 100% sure about the use of request here, is it the correct name? }

Esp
---

I'm not clear what metric could be used for autoscaling by esp.  Cpu is often used for micro services, but many of esp's services are not cpu bound.  I am not familiar with the internal design, but if esp internally maintains queues of requests and responses then those could be used to publish metrics similar to eclccserver and other components.  This requires more input from tony.

Esp has the extra complication that it publishes multiple services, and each of those services should be monitored individually as well as the health of the esp server.  Also with DESDL the list of services is dynamic - e.g. when it is publishing roxie queries.

I think this means it should publish:

service time: [{time-histogram}.request/<service-name>]

as well as

esp-response-time. [{time-histogram}.request]

I suspect keeping track of time spent in dali [ counter.time.dali_request:wait ] would also be useful for fault diagnosis.

The code that accesses LDAP within the security module should publish the standard { time-histogram, request, success, failure } metrics (unless the LDAP server is already publishing these metrics).
Are there are any other services like LDAP that esp relies on?

I am not sure if esp has a fixed node pool for processing queries.  If so it would be useful to included this as a resource metric:

active-queries:  [ count.service.thread.<esp-name>.esp ] - if this is constantly at maximum that would be a candidate for autoscaling.

Q) Should esp be further decomposed so that individual services are running on different esps?  It would allow better auto-scaling per service, but I suspect the current ip:port binding means it wouldn't work.

Roxie
-----

In the deployed-query model this is similar to esp - it publishes multiple independent services.  (The published services are likely to be even more dynamic than the services are in esp.)  Ideally each of these should be monitored individually - it may not even make much sense to combine them.

service time: [{time-histogram}.service/<service-name>]
Number of requests processed [counter.count.service[:success]/<service-name>]
Number of request timeouts [counter.count.service[:timeout]/<service-name>]
Number of request failures [counter.count.service[:failure]/<service-name>]

Also, each of the channels can be thought of as a separate resource.  There will be situation where each channel will be autoscalled separately.

[ counter.count.channel:{requests|inflight|demand}/<channel>.channel ]

Is this sufficient to autoscale?  Do we need another metric that is more dependent on the %time spent processing requests instead [ counter.time.channel ]?  Can the metric servers produce a derived rate metric that is suitable for scaling from [counter.time.channel/<channel>.channel]?

dafilesrv
---------
This is likely to be resurrected to provide authenticated access to HPCC data from external users e.g. spark.

Likely to be similar to monitoring requests to any other service - time, num requests, successes and failures.

(Jake pointed out that you need to take care when scaling some resources - if dafilesrv is not processing enough transactions because the associated disk is slow, adding extra replicas may make the problem worse, rather than better.}

Others? Sasha, eclscheduler, dfuserver?
---------------------------------------

Sasha doesn't really have any requests, but it would be sensible to record how long is spent in each of its services.  (And when they start/stop to an event server.) It might be sensible to have an instances for each service.

eclscheduler is very light-weight  Possibly monitoring the number of events being processed? I'm not convinced anything would be very useful.

dfuserver?  Where does the copying code actually run at the moment?  Is it within esp?  The processing time for requests is likely to be much larger than the metric reporting period.

Metric Servers
==============

(Please point out any misunderstandings...)

How are the metrics that are gathered by the system exposed to the outside world?  The suggestions so far are Prometheus, ELK/Elastic search and a simple file.  Are there any others that are serious contenders?  I suspect the simple file is only useful for debugging, since collating the metrics from multiple files would be significant work.

Prometheus uses a pull model - a http request comes in to a component, and the component responds with a set of metrics.  (It also supports a push model for occasional short-lived components, but it isn't the normal way of working.)
ELK uses a push model - periodically metrics are published to an elastic search index.

It makes sense to allow the target for the metrics to configurable.  As long as there isn't one solution that is obviously the best and likely to stay that way.  (Is prometheus becoming the defacto standard?)

Does it make sense to allow the metrics to be published to multiple sinks?  I am less and less convinced that it does, but we should discuss it.  If not then it may simplify the metric framework design.  Unless there is a clear reason why you would want to report to multiple sinks I would err on the side of not supporting it.

What control should there be over publishing the metrics?  Is there any situation where the set of metrics should be filtered?  If so, how should that be configured?

Do any of the metric sinks require the meta data for the metrics to be published independently of gathering the metrics?  (I don't know enough about elk to know the answer to the question.)  If metadata *is* required separately, then it is hard to see how that fits in with esp/roxie where the published metrics will change dynamically.


Questions:
==========

What does it mean to return the number of active queries, or items on a queue/in-flight?  Is it the instantaneous number at that time or time averaged?  It should be instantaneous - you cannot scale up and down in response to quicker fluctuations, and the response time metrics will provide a better indication of problems with a service.  However, time averaged may be a better solution when monitoring a resource, rather than work done - e.g. the number of active threads since whether they are continually in use is most significant.  The metric server should be able to produce an average within a moving window.

Gathering a metric, especially the histograms, is likely to impose a cost.  We need to check each of the components and their associated metrics to ensure they are valuable, and if  a simpler metric would be sufficient.  E.g. if the times are of the same order as the reporting period it is probably best to report a total rather than a histogram.

How are the metrics configured?  E.g. how are the thresholds supplied for esp or roxie service timings?

Is it useful to allow other metrics to be reported (e.g. timings for the address cleaner), and if so how would they be implemented and integrated?

What metrics should be gathered for all pods?  Cpu, memory, local disk?  Whatever metricserver provides as some defaults?

Need a better idea of how prometheus and other metric servers expect metrics to be defined, and what support for aggregation and publishing derived metrics they support.  The accepted convention for metric names and labels needs more research.

Do we want to support publishing metrics from "short-lived" components?  E.g. address cleaning time from thor?  If so how?  Do we push the metrics?  Allow pull, but push on completion?  Return metrics to the parent pod?  At the end or continually??  Does the calling pod extract them from the workunit stats?

