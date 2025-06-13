for loss in 0 0.01 0.02 0.05 0.1 0.2 0.3
do
    ../../ns3 run "scratch/http1.1_lose/lose --nRequests=50 --errorRate=$loss"
    mv hol_timeline.csv hol_timeline_loss_${loss}.csv
done
